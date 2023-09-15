#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <poll.h>
#include <curl/curl.h>
#include "server.h"
#include "http.h"
#include "buffer.h"
#include "torrent.h"
#include "work.h"
#include "mem.h"

static int __DO_SHUTDOWN = 0;

void handle_sigusr(int no) {
	__DO_SHUTDOWN = 1;
	fprintf(stderr, "SHUTDOWN SIGNAL\n");
}

/* idk some kind of live logging */
void update_server(struct server *s) {
	fprintf(stdout, "Online threads: %d\n", s->th_count);
	struct pth_entry *pt;
	TAILQ_FOREACH(pt, s->th_list, entries)
		fprintf(stdout, "client fd: %d\n", pt->clientfd); 
	fprintf(stdout, "\n");
	return;
}

void cleanup_server(struct server *s, int force) {
	struct pth_entry *p1, *p2;
	int r = -1;
	p1 = TAILQ_FIRST(s->th_list);
	while (p1 != NULL) {
		p2 = TAILQ_NEXT(p1, entries);

		/* ok peekjoin doesn't actually join the thread */
		/* doesn't work on macos try to find some workaround */
		/* altough works on freebsd, and doesnt not work on linux */
#if defined(__FreeBSD__)	
		if (!force)
			r = pthread_peekjoin_np(p1->th, NULL);
#endif
		
		if (r == 0 || force == 1) {
			pthread_join(p1->th, NULL);
			TAILQ_REMOVE(s->th_list, p1, entries);
			free((void *)p1);
			s->th_count--;
		}
		p1 = p2;
	}
	return;
}

int server_do_stats(struct server *s) {
	/* I ain't doing any stats */
	return 0;
}

int shutdown_server(struct server *s) {
	return __DO_SHUTDOWN;
}


void server_stop_threads(struct server *s) {
	struct thread_data	*td;
	struct job_t		*job;
	int			 i;
	/* this should produce a number of STOP jobs one for each thread */
	for (i = 0; i < MAX_THREADS/2; i++) {
		td = mk_td();
		td->sq = NULL;
		td->bp = NULL;
		td_enqueue(s->sq->tlist, td);
		td_wakeup(s->sq->tlist);

		job = mk_job();
		job->type = JOB_T_STOP;
		workq_enqueue_job(s->sq->wq, job);
		workq_wakeup(s->sq);
	}
}

void do_shutdown(struct server *s) {
	/* do not accept any new incomming connection
	   wait for all threads to exit*/
	close(s->fd); /* no more accepts */
	server_stop_threads(s);
	cleanup_server(s, 1);
	update_server(s);
	torrent_stop_pool(s->sq->tl->list);
	buffer_pool_stop_buffers(s->bp);

	/* td data clean */
	td_stop_list(s->sq->tlist);
	td_stop_td_pool(s->tp);
	
	LIST_INIT(s->sq->tl->list);
	free(s->sq->tl->list);
	pthread_mutex_destroy(s->sq->tl->mutex);
	pthread_cond_destroy(s->sq->tl->cond);
	free(s->sq->tl->mutex);
	free(s->sq->tl->cond);
	/* the workq should be clean since i forced the join */
	TAILQ_INIT(s->sq->wq->wq);
	pthread_mutex_destroy(s->sq->wq->mtx);
	free(s->sq->wq->mtx);
	free(s->sq->wq->wq);
	
	free(s->sq->wq);
	free(s->sq->tl);

	/* time to free everything */
	free(s->sq);
	assert(s->th_count == 0);
	TAILQ_INIT(s->th_list);
	free(s->th_list);
	free(s); /* goodbye old friend */
}

int update_max_try(struct server *s) {
	/* sleeeeep */
	sleep(TIMEOUT);
	return ++s->tries;
}

int set_shutdown(struct server *s, int status) {
	// update server shutdown
	return 0;
}

int runq_events(struct server *s) {
	/* printf("waiting for a client to connect\n"); */
	struct sockaddr_in	 clientaddr;
	struct thread_data	*thread_data;
	struct pollfd		 pfd[1];
	int			 client, pollr;
	
	unsigned int		b      = sizeof(clientaddr);

	pfd[0].fd      = s->fd;
	pfd[0].events  = POLLIN;
	pfd[0].revents = 0;

	if (workq_safepeek(s->sq->wq) != 0) {
		pollr = poll(pfd, 1, 1000);
		if (pollr == 0)
			return 1;
		if (pollr == -1)
			return -1;
	}
	else {
		pollr = poll(pfd, 1, -1);
		if (pollr == -1)
			if (EINTR == errno) {
				/* return from sigint */
				return -1;
			}
	}
	
	while ((client = accept(s->fd,(struct sockaddr *)&clientaddr, &b)) == -1) {
		if (errno != EINTR) {
			perror("accept: ");
			exit(0);
		}
		fprintf(stderr, "accept was interrupted\n");
	}

	/* probably a fixed amount of td should be enough */
	thread_data = td_pool_get_td(s->tp);
	// thread_data = mk_td();
	thread_data->client = client;
	thread_data->sq	    = s->sq;
	thread_data->bp     = s->bp;
	memcpy(&thread_data->clientaddr, &clientaddr, sizeof(struct sockaddr_in));

	td_enqueue(s->sq->tlist, thread_data);
	td_wakeup(s->sq->tlist);
	return 0;
}

struct server *build_server_connection(void) {

	fprintf(stdout, "Building server, hang in there...\n");
	int			 server;
	struct sockaddr_in	 addr;
	struct in_addr		 netaddr;
	
	inet_aton("0.0.0.0", &netaddr);
	
	addr.sin_family	= AF_INET;
	addr.sin_port	= htons(10010);
	addr.sin_addr	= netaddr;
	
	if ((server = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket: ");
		exit(0);
	}
	if (bind(server, (struct sockaddr *) &addr,
		 sizeof(struct sockaddr_in)) == -1) {
		perror("bind: ");
		exit(0);
	}
	/* sysctl somaxconn */
	if (listen(server, 4096) == -1) {
		perror("listen: ");
		exit(0);
	}

	struct server	*s = xmalloc(sizeof(struct server));
	s->fd		   = server;
	s->tries	   = 0;
	fprintf(stdout, "Server built, enjoy...\n");
	return s;
}

void server_init_server_list(struct server *s) {
	s->th_list = xmalloc(sizeof(struct pth_list));
	s->th_count = 0;
	TAILQ_INIT(s->th_list);
}

void server_init_threads(struct server *s) {
	/* spawn worker threads */

	struct pth_entry	*tids;
	int			 i, tries;
	/* these are the work threads */
	tids = xmalloc(sizeof(struct pth_entry) * MAX_THREADS);
	for (i = 0, tries = 0; i < MAX_THREADS/2; i++) {
		if (pthread_create(&(tids[i].th), NULL, handle_jobs, s->sq) == -1) {
			perror("pthread_create: ");
			i--;
			if (++tries == MAX_TRY) {
				fprintf(stderr, "fatal error: couldn't spawn threads\n");
				exit(1);
			}
			continue;
		}

		/* add to the thereads queue */
		tids[i].clientfd = -1;
		s->th_count++;
		TAILQ_INSERT_TAIL(s->th_list, &(tids[i]), entries);
		fprintf(stdout, "thread %03d spawned\n", i);

		/* producer threads */
		if (pthread_create(&(tids[i+MAX_THREADS/2].th), NULL, handle_req, s->sq->tlist) == -1) {
			perror("pthread_create: ");
			i--;
			if (++tries == MAX_TRY) {
				fprintf(stderr, "fatal error: couldn't spawn threads\n");
				exit(1);
			}
			continue;
		}
		tids[i+MAX_THREADS/2].clientfd = -1;
		s->th_count++;
		TAILQ_INSERT_TAIL(s->th_list, &(tids[i+MAX_THREADS/2]), entries);
		fprintf(stdout, "thread %03d spawned\n", i+MAX_THREADS/2);
	}
	
	fprintf(stdout, "there are %03d active threads\n", s->th_count);
}

void server_init_globals(struct server *s) {
	struct torrent_list_t	*tl;
	struct workqueue_t	*wq;
	struct td_list_t *tlist;
	
	/* torrent list */
	tl	    = xmalloc(sizeof(*tl));
	tl->mutex   = xmalloc(sizeof(pthread_mutex_t));
	tl->cond    = xmalloc(sizeof(pthread_cond_t));
	tl->list    = xmalloc(sizeof(struct torrent_list));

	pthread_mutex_init(tl->mutex, NULL);
	pthread_cond_init(tl->cond, NULL);
	LIST_INIT(tl->list);
	tl->working = 0;

	/* workqueue */
	wq	 = xmalloc(sizeof(*wq));
	wq->mtx	 = xmalloc(sizeof(pthread_mutex_t));
	wq->wq	 = xmalloc(sizeof(struct workqueue));
	wq->cond = xmalloc(sizeof(pthread_cond_t));

	pthread_cond_init(wq->cond, NULL);
	pthread_mutex_init(wq->mtx, NULL);
	TAILQ_INIT(wq->wq);

	/* thread_data list */
	tlist	       = xmalloc(sizeof(*tlist));
	tlist->td_list = xmalloc(sizeof(struct td_list));
	tlist->cond    = xmalloc(sizeof(pthread_cond_t));
	tlist->mtx     = xmalloc(sizeof(pthread_mutex_t));
	pthread_cond_init(tlist->cond, NULL);
	pthread_mutex_init(tlist->mtx, NULL);
	LIST_INIT(tlist->td_list);

	s->sq	     = xmalloc(sizeof(struct server_q));
	s->sq->wq    = wq;
	s->sq->tl    = tl;
	s->sq->tlist = tlist;

	/* buffer pool */
	s->bp = xmalloc(sizeof(*s->bp));
	buffer_pool_init(s->bp);

	/* thread pool */
	s->tp = xmalloc(sizeof(*s->tp));
	td_pool_init(s->tp);
	
}

void main_server(void) {
	struct server *s;
	
	s = build_server_connection();
	/* buffer pool needed, too many allocs around */
	server_init_server_list(s);
	server_init_globals(s);
	server_init_threads(s);
	torrent_init_cache_pool();
	do {
		server_do_stats(s);
		if (runq_events(s) == -1)
			sleep(1);
		/* update_server(s); */

		/* on macos skip */		
		cleanup_server(s, 0);
	} while (!shutdown_server(s));
	do_shutdown(s);
	printf("Shutting down!\n");
}


int main(int argc, char *argv[]) {
	/* config reader, do the necessary */
	signal(SIGPIPE, SIG_IGN);
	signal(SIGUSR1, handle_sigusr);
	signal(SIGUSR2, handle_sigusr);
	signal(SIGTERM, handle_sigusr);
	main_server();
	return 0;
}

/* do i really need this? */
int do_work(struct server *s) {
	runq_events(s);
	return 0;
}

void *handle_jobs(void *ptr) {
	struct server_q *sq;
	struct job_t		*job;
	int rv;
	sq = (struct server_q *)ptr;
	/* check if I have something to do... */
	while ((job = workq_wait_and_deq_job(sq)) != NULL) {
		if ((rv = work_process_job(job)) == JOB_T_STOP) {
			job_complete(job); break;
		}
		if (http_handle_write(job) == -1)
			fprintf(stderr, "Job had problem while completing..");
		job_complete(job);
	}
	pthread_exit(0);
}

void *handle_req(void *ptr) {
	struct thread_data	*td;
	struct server_q		*sq;
	struct buffer		*b;
	struct buffer_pool	*bp;
	struct td_list_t	*tlist;
	struct td_pool		*tp;
	tlist = (struct td_list_t *)ptr;
	while ((td = td_wait_and_dequeue(tlist)) != NULL) {
		/* stop the thread */
		if (td_null(td)) {
			td_pool_join_td(td->tp, td);
			break;
		}
		
		sq = td->sq;
		bp = td->bp;
		tp = td->tp;
		b  = buffer_pool_get_buffer(bp);
		if (http_handle_read(td, b) == -1) {
			close(td->client);
			buffer_pool_join_buffer(td->bp, b);
			td_pool_join_td(tp, td);
			/* td pool join */
			continue;
		}
		buffer_pool_join_buffer(td->bp, b);
		workq_wakeup(sq);
	}

	/* stop queues & related */
	pthread_exit(0);
}
