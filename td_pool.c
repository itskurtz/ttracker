#include <stdio.h>
#include <stdlib.h>
#include "td_pool.h"

void td_pool_init(struct td_pool *td_pool) {
	struct td_pool_entry *tde;
	pthread_mutex_init(&td_pool->mutex, NULL);
	pthread_cond_init(&td_pool->cond, NULL);

	td_pool->pool = xmalloc(sizeof(struct td_pool_list));
	td_pool->inuse = 0;
	td_pool->num = 0;

	LIST_INIT(td_pool->pool);
	td_pool_populate_pool(td_pool);
}


static void lock_pool(struct td_pool *td_pool) {
	pthread_mutex_lock(&td_pool->mutex);
}

static void unlock_pool(struct td_pool *td_pool) {
	pthread_mutex_unlock(&td_pool->mutex);
}

static void wait_for_free_td(struct td_pool *tdp) {
	while (tdp->inuse == tdp->num)
		pthread_cond_wait(&tdp->cond, &tdp->mutex);
}

int td_pool_populate_pool(struct td_pool *tdp) {
	struct td_pool_entry *tde;
	int i;

	if (tdp->num == TD_POOL_MAX_TD)
		return TD_POOL_WAIT;

	for (i = 0; i < TD_POOL_AMP; i++) {
		tde = xmalloc(sizeof(struct td_pool_entry));
		tde->thread_data = xmalloc(sizeof(struct thread_data));
		tde->thread_data->tp = tdp;
		tde->status = TD_POOL_STATUS_FREE;
		LIST_INSERT_HEAD(tdp->pool, tde, entries);
	}
	tdp->num += TD_POOL_AMP;
	return TD_POOL_OK;
}

struct thread_data *td_pool_get_td(struct td_pool *tdp) {
	struct td_pool_entry *tde;
	int r, inuse;
	lock_pool(tdp);
	if (tdp->num - tdp->inuse <= 0) {
		r = td_pool_populate_pool(tdp);
		if (r == TD_POOL_WAIT)
			wait_for_free_td(tdp);
	}

	LIST_FOREACH(tde, tdp->pool, entries) {
		if (tde->status == TD_POOL_STATUS_FREE) {
			tde->status = TD_POOL_STATUS_INUSE;
			tdp->inuse++;
			unlock_pool(tdp);
			return tde->thread_data;
		}
	}
	unlock_pool(tdp);
	return NULL;
}

void td_pool_join_td(struct td_pool *tdp, struct thread_data *td) {
	struct td_pool_entry *tde;
	lock_pool(tdp);
	LIST_FOREACH(tde, tdp->pool, entries)
		if (tde->thread_data == td) {
			assert(tde->status == TD_POOL_STATUS_INUSE);
			/* the application will handle the zero of data  */
			tde->status = TD_POOL_STATUS_FREE;
			tdp->inuse--;
			pthread_cond_signal(&tdp->cond);
			unlock_pool(tdp);
			return;
		}

	/* never reached */
	unlock_pool(tdp);
	fprintf(stderr, "thread_data not found in pool\n");
	abort();
}

void td_stop_td_pool(struct td_pool *tdp) {
	struct td_pool_entry *tde, *tdu;
	lock_pool(tdp);
	tde = LIST_FIRST(tdp->pool);
	while (tde != NULL) {
		tdu = LIST_NEXT(tde, entries);
		free(tde->thread_data);
		free(tde);
		tde = tdu;
	}
	unlock_pool(tdp);
	LIST_INIT(tdp->pool);
	free(tdp->pool);

	pthread_mutex_destroy(&tdp->mutex);
	pthread_cond_destroy(&tdp->cond);
	free(tdp);
}
