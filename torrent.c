#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <string.h>
#include <assert.h>
/* since i dont need it anywhere else (for now at least) i can just get it here */
#include "torrent.h"
#include "db.h"
#include "bncode.h"
#include "server.h"
#include "mem.h"

void torrent_init_cache_pool(void) {
	if (db_connect("./test.db") == -1) {
		fprintf(stderr, "Error connecting to db\nAborting...\n");
		exit(1);
	}
}

void torrent_stop_pool(struct torrent_list *list) {
	struct torrent_entry *tp1, *tp2;
	db_disconnect();
	/* freeing all torrents */
	tp1 = LIST_FIRST(list);
	while (tp1 != NULL) {
		tp2 = LIST_NEXT(tp1, entries);
		LIST_REMOVE(tp1, entries);
		free_torrent(tp1);
		free(tp1);
		tp1 = tp2;
	}
}

/* i need a pool of allocated buffers to clean at the end */
void torrent_msg_failure_missing_hash(struct buffer *b) {
	buffer_append_string_to_buffer(b, "d14:failure reason29:Missing \"info_hash\" parametere");
}

/* compared to the old version this is fast af (look git history) */
void torrent_bencode_torrent(struct torrent_entry *t, char *out, int out_size) {
	struct peer_entry	*pt;
	char			*p;
	int			 i, written;
	written = i = 0;
	p = out;
	/* make a dict for each peer and insert it into list */
	/* i need some tunable here but for now default is ok */
	/* now i must wrap everything under {interval: 30, peers: [list of dicts]} */
	written += snprintf(p, out_size - written, "d8:completei%de10:incompletei%de8:intervali%de5:peersl", t->seeders, t->leechers, 30);	
	LIST_FOREACH(pt, t->peers, entries) {
		if (i >= T_MAX_NUMWANT)
			break;
		written += snprintf(p+written, out_size - written, "d4:porti%lde7:peer_id%zu:%s2:ip%zu:%se",
				   pt->port, strlen(pt->peer_id), pt->peer_id,
				   strlen(pt->ip), pt->ip);
		i++;
	}
	snprintf(p+written, out_size - written, "ee"); /*close list and dict */
}

struct torrent_entry *torrent_do_lookup_torrent_info_hash(char *info_hash, struct torrent_list *tl) {
	struct torrent_entry *pt;
	LIST_FOREACH(pt, tl, entries) {
		if (memcmp(info_hash, pt->info_hash,
			   sizeof(char) * TORRENT_HASH_LEN) == 0) {
			return pt;
		}
	}
	return NULL;
}

/* assertion: the torrent must be present in db */
struct torrent_entry *torrent_lookup_torrent(char *info_hash, struct torrent_list_t *tl) {
	/* i could just ask a thread to fill up the cache
	   and wait on condition! */
	struct torrent_entry	*te;
	int			 tid;
	// srandom(time(NULL)), tid = random();
	pthread_mutex_lock(tl->mutex);
	/* even if someone works on the torrent this should not be a problem
	   since the info hash is never touched */
	te = torrent_do_lookup_torrent_info_hash(info_hash, tl->list);
	/* */
	if (__builtin_expect(te == NULL, 0)) {
		/* printf("torrent not found.. adding to torrent list\n"); */
		/* this might block the application */
		if ((tid = db_query_lookup_torrent(info_hash)) == -1) {
			pthread_mutex_unlock(tl->mutex);
			return NULL;
		}
		/* not quite right */
		te	       = mk_torrent();
		strncpy(te->info_hash, info_hash, TORRENT_HASH_LEN);
		te->id	       = tid;
		te->peer_count = 0;
		LIST_INSERT_HEAD(tl->list, te, entries);
	}
	/* else { */
	/* 	printf("torrent found.. no need to add, returning\n"); */
	/* } */
	pthread_mutex_unlock(tl->mutex);
	return te;
}

/* added after rel's rewrite  */
struct peer_entry *mk_peer(void) {
	struct peer_entry *p;
	p = malloc(sizeof(struct peer_entry));
	if (__builtin_expect(p == NULL, 0)) {
		perror("malloc: ");
		exit(1);
	}
	return p;
}

void free_peer(struct peer_entry *pe) {
	/* if pe is null then its ur problem!!! */
	free(pe);
}

struct torrent_entry *mk_torrent(void) {
	struct torrent_entry *p;
	p	      = xmalloc(sizeof(struct torrent_entry));
	/* init pthread */
	p->peer_rwlk  = xmalloc(sizeof(pthread_rwlock_t));
	p->peers      = xmalloc(sizeof(struct peer_list));
	p->leechers   = 0;
	p->peer_count = 0;
	p->complete   = 0;
	p->seeders    = 0;
	LIST_INIT(p->peers);
	pthread_rwlock_init(p->peer_rwlk, NULL);
	return p;
}

struct torrent_entry *
torrent_search_torrent(	struct torrent_entry *te,
			struct torrent_list *torrent_l) {
	
	struct torrent_entry *tp = NULL;
	
	LIST_FOREACH(tp, torrent_l, entries)
		if (tp->id == te->id)
			return tp;
	return NULL;
}


struct peer_entry *
peer_search_peer(struct peer_entry *pe,
		 struct peer_list *peer_l) {
	struct peer_entry *pt = NULL;
	/* critical section */
	LIST_FOREACH(pt, peer_l, entries)
		if (strcmp(pt->ip, pe->ip) == 0 && (pt->port == pe->port))
			return pt;
	return NULL;
}

void torrent_lock_list(struct torrent_list_t *tlist) {
	pthread_mutex_lock(tlist->mutex);
	tlist->working++;
	pthread_cond_signal(tlist->cond);
	pthread_mutex_unlock(tlist->mutex);
}

void torrent_unlock_list(struct torrent_list_t *tlist) {
	pthread_mutex_lock(tlist->mutex);
	tlist->working--;
	pthread_cond_signal(tlist->cond);
	pthread_mutex_unlock(tlist->mutex);	
}

void torrent_list_workon(struct torrent_list_t *list) {
	torrent_lock_list(list);
}

void torrent_list_jobdone(struct torrent_list_t *list) {
	torrent_unlock_list(list);
}


/* prob I dont need this */
void torrent_remove_torrent(struct torrent_entry *te, struct torrent_list_t *torrent_l) {

	pthread_mutex_lock(torrent_l->mutex);
	while (torrent_l->working > 0)
		pthread_cond_wait(torrent_l->cond, torrent_l->mutex);
	if (__builtin_expect(torrent_l->working < 0, 0)) {
		fprintf(stderr, "fatal error\n");
		exit(1);
	}
	LIST_REMOVE(te, entries);
	pthread_mutex_unlock(torrent_l->mutex);
}

char *torrent_add_peer_and_return_list(struct torrent_entry *te, struct peer_entry *pe) {
	struct peer_entry *pp;
	pthread_rwlock_wrlock(te->peer_rwlk);

	pp = peer_search_peer(pe, te->peers);
	if (pp == NULL) {
		LIST_INSERT_HEAD(te->peers, pe, entries);
		te->peer_count++;

		if (pe->left == 0) te->seeders++;
		else te->leechers++;
	}

	/* here i have to to a peer_search_peer again */
	/* 128 is just a big number, nothing special about that */
	char *out = calloc(128 * te->peer_count, sizeof(char));
	assert(out != NULL);
	/* i need to return a list of peers */
	torrent_bencode_torrent(te, out, 128 * te->peer_count);
	pthread_rwlock_unlock(te->peer_rwlk);
	return out;
}

/* depending on action prob o */
char *torrent_update_peer_and_return_list(struct torrent_entry *te, struct peer_entry *pe, int action) {
	char *out;
	/* do something on peer */

	if (!action) 	pthread_rwlock_rdlock(te->peer_rwlk);
	else 	pthread_rwlock_wrlock(te->peer_rwlk);
	
	switch (action) {
	case T_COMPLETE:
		te->complete++;
		te->seeders++;
		break;
	case T_START:
		/* not always true */
		if (pe->left == 0)
			te->seeders++;
		else
			te->leechers++;
	default:
		break;
	}

	out = calloc(128 * te->peer_count, sizeof(char));
	assert(out != NULL);
	torrent_bencode_torrent(te, out, 128 * te->peer_count);
	pthread_rwlock_unlock(te->peer_rwlk);
	return out;
}

void torrent_remove_peer(struct torrent_entry *te, struct peer_entry *pe) {

	pthread_rwlock_wrlock(te->peer_rwlk);
	/* assert pe is correct */
	LIST_REMOVE(pe, entries);
	/* gotta check if peer has completed or not */
	te->peer_count--;
	if (pe->left == 0) {
		te->seeders--;
		te->complete--;
	}
	else
		te->leechers--;
	
	pthread_rwlock_unlock(te->peer_rwlk);
}

void free_torrent(struct torrent_entry *te) {
	/* i suppose no one works on this */
	struct peer_entry *pe, *pn;
	pe = LIST_FIRST(te->peers);
	while (pe != NULL) {
		pn = LIST_NEXT(pe, entries);
		free_peer(pe);
		pe = pn;
	}
	LIST_INIT(te->peers);
	free(te->peers);
	pthread_rwlock_destroy(te->peer_rwlk);
	free(te->peer_rwlk);
}

struct scrape_ctx *scrape_start_ctx() {
	struct scrape_ctx *ctx;
	ctx	  = xmalloc(sizeof(*ctx));
	LIST_INIT(&ctx->list);
	ctx->size = 0;
	return ctx;
}

void scrape_add_torrent(struct scrape_ctx *ctx, struct torrent_entry *te) {
	struct scrapeentry_t *sep;
	sep = xmalloc(sizeof(*sep));
	pthread_rwlock_rdlock(te->peer_rwlk);
	sep->seeders	= te->seeders;
	sep->leechers	= te->leechers;
	sep->complete   = te->complete;
	sep->peer_count = te->peer_count;
	memcpy(sep->info_hash, te->info_hash, TORRENT_HASH_LEN * sizeof(char));
	pthread_rwlock_unlock(te->peer_rwlk);
	LIST_INSERT_HEAD(&ctx->list, sep, entries);
	ctx->size++;
}

void scrape_stop_ctx(struct scrape_ctx *ctx) {
	struct scrapeentry_t *sep, *sp;
	sp = LIST_FIRST(&ctx->list);
	while (sp != NULL) {
		sep = LIST_NEXT(sp, entries);
		free(sp);
		sp = sep;
	}
	LIST_INIT(&ctx->list);
	free(ctx);
}

int bencode_scrape_ctx(struct scrape_ctx *ctx, char *out) {
	int written, i;
	char *p, *str;
	struct scrapeentry_t *se;
	unsigned char cur_hash[20];
	written	      = 0;
	p	      = out;
	written	     += sprintf(p, "d5:filesd");
	/* make a dict for each entry */
	LIST_FOREACH(se, &(ctx->list), entries) {
		str = se->info_hash;
		/* ugly encoding but does the job*/
		for (i = 0; i < 20; i++) {
			sscanf(str, "%02hhx", &cur_hash[i]);
			str+=2;
		}
		written += sprintf(p+written, "20:");
		memcpy(p+written, cur_hash, sizeof(unsigned char) * 20);
		written += 20;
		written += sprintf(p+written, "d8:completei%de10:downloadedi%de10:incompletei%dee",
				   se->seeders, se->complete, se->leechers);
		memset(cur_hash, 0, sizeof(unsigned char) * 20);
	}
	written += sprintf(p+written, "ee");
	return written;
}
