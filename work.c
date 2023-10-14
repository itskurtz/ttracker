#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/queue.h>
#include <assert.h>
#include "base.h"
#include "buffer_pool.h"
#include "torrent.h"
#include "http.h"
#include "work.h"
#include "mem.h"

struct job_t *mk_job() {
	struct job_t *job = xmalloc(sizeof(struct job_t));
	return job;
}

struct job_t *workq_wait_and_deq_job(struct server_q *sq) {
	struct job_t *job;
	pthread_mutex_lock(sq->wq->mtx);
	while ((job = workq_unsafe_deq_job(sq->wq)) == NULL)
		pthread_cond_wait(sq->wq->cond, sq->wq->mtx);
	pthread_mutex_unlock(sq->wq->mtx);
	return job;
}

void workq_wakeup(struct server_q *sq) {
	pthread_mutex_lock(sq->wq->mtx);
	pthread_cond_signal(sq->wq->cond);
	pthread_mutex_unlock(sq->wq->mtx);
}

void workq_enqueue_job(struct workqueue_t *wq, struct job_t *job) {
	struct work_entry *we;
	we = xmalloc(sizeof(struct work_entry));

	pthread_mutex_lock(wq->mtx);
	we->job = job;

	TAILQ_INSERT_TAIL(wq->wq, we, entries);

	pthread_mutex_unlock(wq->mtx);
}

int workq_safepeek(struct workqueue_t *wq) {
	struct work_entry *we;
  	pthread_mutex_lock(wq->mtx);
	/* look for the 1st job in queue */
	we = TAILQ_FIRST(wq->wq);
	if (we) {
		pthread_mutex_unlock(wq->mtx);
		return 1;
	}
	pthread_mutex_unlock(wq->mtx);
	return 0;
}

struct job_t *workq_unsafe_deq_job(struct workqueue_t *wq) {
	struct job_t		*job;
	struct work_entry	*we;
	
	/* look for the 1st job in queue */
	we = TAILQ_FIRST(wq->wq);
	if (we) {
		job = we->job;
		TAILQ_REMOVE(wq->wq, we, entries);
		free(we);
		return job;
	}
	
	/* empty queue */
	return NULL;
}

struct job_t *workq_dequeue_job(struct workqueue_t *wq) {
	struct job_t		*job;
	struct work_entry	*we;
	
	pthread_mutex_lock(wq->mtx);
	/* look for the 1st job in queue */
	we = TAILQ_FIRST(wq->wq);
	if (we) {
		job = we->job;
		TAILQ_REMOVE(wq->wq, we, entries);
		free(we);
		pthread_mutex_unlock(wq->mtx);
		return job;
	}
	pthread_mutex_unlock(wq->mtx);
	
	/* empty queue */
	return NULL;
}

/* main scrape routine */
static void _job_handle_scrape(struct job_t *job) {
	char			*scrape_list;
	struct peer_entry	 pe, *pp;
	struct hashentry_t	*hep;
	struct torrent_entry	*torrent;
	struct scrape_ctx	*sctx;
	int scrape_len;

	sctx = scrape_start_ctx();
	job->peer_req = http_build_peer_req_hash_list(job->http_kv_list);
	torrent_list_workon(job->thread_data->sq->tl);
	LIST_FOREACH(hep, job->peer_req->hashlist, entries) {
		torrent = torrent_lookup_torrent(hep->info_hash, job->thread_data->sq->tl);
		if (__builtin_expect(torrent == NULL, 0)) {
			fprintf(stderr, "NOT FOUND -> info_hash: %s\n", hep->info_hash);
			torrent_list_jobdone(job->thread_data->sq->tl);
			scrape_stop_ctx(sctx);
			torrent_msg_failure_missing_hash(job->out);
			return;
		}
		scrape_add_torrent(sctx, torrent);
	}
	torrent_list_jobdone(job->thread_data->sq->tl);
	scrape_list = calloc(128 * sctx->size, sizeof(char));
	scrape_len = bencode_scrape_ctx(sctx, scrape_list);
	buffer_write_to_buffer(job->out, scrape_list, scrape_len);
	scrape_stop_ctx(sctx);
	free(scrape_list);
}


__attribute__((hot))
static void _job_handle_announce(struct job_t *job) {
	struct peer_entry	 pe, *pp, *pep;
	struct torrent_entry	*torrent;
	char			*peer_list;
	int			 sanitize_error;
	
	/* ok so at this time I can only handle announces */
	/* not true, I can handle scrapes aswell*/
	job->peer_req = http_build_peer_req(job->http_kv_list);
	if (sanitize_peer_req(job->peer_req, &sanitize_error) == -1) {
		/*   */
		switch(sanitize_error) {
		case E_INFO_HASH:
			torrent_msg_failure_generic(
				job->out,
				"Corrupted hash", strlen("Corrupted hash"));
			break;
		case E_PEER_ID:
			torrent_msg_failure_generic(
				job->out,
				"Invalid peer_id", strlen("Invalid peer_id"));
			break;
		case E_PORT:
			torrent_msg_failure_generic(
				job->out,
				"Invalid port", strlen("Invalid port"));
			break;
		case E_DOWNLOADED:
			torrent_msg_failure_generic(
				job->out,
				"Invalid downloaded value", strlen("Invalid downloaded value"));
			break;
		case E_UPLOADED:
			torrent_msg_failure_generic(
				job->out,
				"Invalid uploaded value", strlen("Invalid uploaded value"));
			break;
		case E_LEFT:
			torrent_msg_failure_generic(
				job->out,
				"Invalid left value", strlen("Invalid left value"));
			break;
		case E_EVENT:
			torrent_msg_failure_generic(
				job->out,
				"Invalid event", strlen("Invalid event"));
			break;
		default:
			torrent_msg_failure_generic(
				job->out,
				"Generic event", strlen("Generic event"));			
			break;
		}

		return;
	}
	
	/* if present in db or whatsoever */
	torrent_list_workon(job->thread_data->sq->tl); /* lock torrent removal */
	torrent = torrent_lookup_torrent(job->peer_req->info_hash, job->thread_data->sq->tl);
	if (__builtin_expect(torrent == NULL, 0)) {
		torrent_list_jobdone(job->thread_data->sq->tl);
		torrent_msg_failure_missing_hash(job->out);
		return;
	}

	/* I need the pair IP-PORT & left */
	get_peer_info(&job->thread_data->clientaddr, pe.ip);
	pe.port = job->peer_req->port;
	strcpy(pe.peer_id, job->peer_req->peer_id);
	pe.left = job->peer_req->left;
	/* lock torrent read */
	/* if found this should be torrent work on peer */
	if ((pp = peer_search_peer(&pe, torrent->peers)) == NULL) {
		if (strncmp(job->peer_req->event, "start", 4) == 0) {
			pep = mk_peer();
			memcpy(pep, &pe, sizeof(struct peer_entry));
			/* torrent_add_peer_and_return_list will do a search aswell */
			peer_list = torrent_add_peer_and_return_list(torrent, pep);
			if (peer_list != NULL) {
				torrent_list_jobdone(job->thread_data->sq->tl);
				buffer_append_string_to_buffer(job->out, peer_list);
				free(peer_list);
			}
			/* return all the peers */
		}
		/* you can't stop a torrent whose peer is not in list */
		/* else if (strncmp(job->peer_req->event, "stop", 4) == 0) { */
		/* 	torrent_list_jobdone(job->thread_data->sq->tl); */
		/* 	// torrent_remove_peer(torrent, pp); */
		/* } */
		else {
			/* probably completed */
			torrent_list_jobdone(job->thread_data->sq->tl);
			/* printf("wrong event my friend\n"); */
		}
	}
	else {
		/* it can only be stop or completed, if stopped remove
		   else just do a 'scrape'
		 */
		if (strncmp(job->peer_req->event, "stop", 4) == 0) {
			/* removing peer */
			peer_list = torrent_update_peer_and_return_list(torrent, pp, T_STOP);
			torrent_remove_peer(torrent, pp);
			torrent_list_jobdone(job->thread_data->sq->tl);
			buffer_append_string_to_buffer(job->out, peer_list);
			free(peer_list);
			free_peer(pp);
		}
		else if (strncmp(job->peer_req->event, "completed", 4) == 0) {
			/* this must be fixed */
			peer_list = torrent_update_peer_and_return_list(torrent, pp, T_COMPLETE);
			torrent_list_jobdone(job->thread_data->sq->tl);
			buffer_append_string_to_buffer(job->out, peer_list);
			free(peer_list);
		}
		else /* (strncmp(job->peer_req->event, "completed", 4))  */ {
			/* completed ok */
			peer_list = torrent_update_peer_and_return_list(torrent, pp, T_NULL);
			torrent_list_jobdone(job->thread_data->sq->tl);
			buffer_append_string_to_buffer(job->out, peer_list);
			free(peer_list);
		}
	}	
}

/* main work routine */
int work_process_job(struct job_t *job) {
	if (job->type == JOB_T_ANNOUNCE) {
		_job_handle_announce(job);
		return JOB_T_ANNOUNCE;
	}
	if (job->type == JOB_T_SCRAPE) {
		_job_handle_scrape(job);
		return JOB_T_SCRAPE;
	}
	return JOB_T_STOP;
}

void job_complete(struct job_t *job) {

	struct http_kv_entry *keyval, *np;
	struct hashentry_t *hep, *hpp;

	if (job->type == JOB_T_STOP) {
		free(job); return;
	}
	
	/* empty the whole list */
	np = TAILQ_FIRST(job->http_kv_list);
	while (np != NULL) {
		keyval = TAILQ_NEXT(np, entries);
		// printf("removing key: %s, val: (%s) \n", np->data.key, np->data.val);
		free(np);
		np = keyval;
	}
	
	/* i dont free job->peer since I need it in the torrent list */
	TAILQ_INIT(job->http_kv_list);
	free(job->http_kv_list);
	if (job->peer_req->hashlist != NULL) {
		/* free the list */
		hep = LIST_FIRST(job->peer_req->hashlist);
		while (hep != NULL) {
			hpp = LIST_NEXT(hep, entries);
			free(hep);
			hep = hpp;
		}
		LIST_INIT(job->peer_req->hashlist);
		free(job->peer_req->hashlist);
	}
	free(job->peer_req);
	close(job->thread_data->client);
	buffer_pool_join_buffer(job->thread_data->bp, job->out);
	
	/* this call does not free td->server & related */
	job->thread_data->sq = NULL; /* valgrind complaing about this one */
	/* free(job->thread_data); */
	td_pool_join_td(job->thread_data->tp, job->thread_data);
	job->thread_data = NULL;
	free(job);
}

/* thread data list part */
struct thread_data *mk_td() {
	struct thread_data *td;
	td = xmalloc(sizeof(*td));
	return td;
}


void td_stop_list(struct td_list_t *tlist) {
	/* must be called after each thread has joined */
	struct td_entry *te, *tp;
	pthread_mutex_destroy(tlist->mtx);
	pthread_cond_destroy(tlist->cond);
	free(tlist->mtx);
	free(tlist->cond);

	te = LIST_FIRST(tlist->td_list);
	while (te != NULL) {
		tp = LIST_NEXT(te, entries);
		if (te->td != NULL)
			free(te->td);
		LIST_REMOVE(te, entries);
		free(te);
		te = tp;
	}
	LIST_INIT(tlist->td_list);
	free(tlist->td_list);
	free(tlist);
}

void td_enqueue(struct td_list_t *tlist, struct thread_data *td) {
	struct td_entry *te;
	te = xmalloc(sizeof(*te));
	/* max capacity */
	pthread_mutex_lock(tlist->mtx);
	te->td = td;
	LIST_INSERT_HEAD(tlist->td_list, te, entries);
	/* i signal the waiting threads */
	pthread_mutex_unlock(tlist->mtx);
}

void td_wakeup(struct td_list_t *tlist) {
	pthread_mutex_lock(tlist->mtx);
	pthread_cond_signal(tlist->cond); /* wake up threads */
	pthread_mutex_unlock(tlist->mtx);
}

struct thread_data *td_list_unsafe_deq_td(struct td_list *tdlist) {
	struct td_entry		*te;
	struct thread_data	*td;
	te = LIST_FIRST(tdlist);
	if (te) {
		LIST_REMOVE(te, entries);
		td = te->td;
		free(te);
		return td;
	}
	return NULL;
}

struct thread_data *td_wait_and_dequeue(struct td_list_t *tlist) {
	struct thread_data *td;
	pthread_mutex_lock(tlist->mtx);
	while ((td = td_list_unsafe_deq_td(tlist->td_list)) == NULL)
		pthread_cond_wait(tlist->cond, tlist->mtx);
	pthread_mutex_unlock(tlist->mtx);
	return td;
}

int td_null(struct thread_data *td) {
	if (td->sq == NULL || td->bp == NULL)
		return 1;
	return 0;
}
