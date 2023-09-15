#include <stdio.h>
#include <stdlib.h>
#include "buffer_pool.h"

/* the buffer pool is connected to the threadpool */

static void wait_for_free_buffer(struct buffer_pool *bp) {
	while (bp->inuse == bp->num)
		pthread_cond_wait(&bp->cond, &bp->mutex);
}

static void lock_pool(struct buffer_pool *bp) {
	pthread_mutex_lock(&bp->mutex);
}
	
static void unlock_pool(struct buffer_pool *bp) {
	pthread_mutex_unlock(&bp->mutex);
}

void buffer_pool_init(struct buffer_pool *bp) {
	/* starts 4 buffers */
	struct buff_pool_entry *bpe;
	pthread_mutex_init(&bp->mutex, NULL);
	pthread_cond_init(&bp->cond, NULL);
	
	bp->pool  = xmalloc(sizeof(struct buff_pool_list));
	bp->inuse = 0;
	bp->num	  = 0;

	LIST_INIT(bp->pool);
	buffer_pool_populate_pool(bp);
	
}

int buffer_pool_populate_pool(struct buffer_pool *bp) {
	
	struct buff_pool_entry *bpe;
	int i;
	if (bp->num == MAX_BUFFERS)
		return BUFF_WAIT;
	
	for (i = 0; i < BUFFER_AMP; i++) {
		bpe	    = xmalloc(sizeof(struct buff_pool_entry));
		bpe->b	    = mk_buffer();
		bpe->status = STATUS_FREE;
		LIST_INSERT_HEAD(bp->pool, bpe, entries);
	}
	bp->num += BUFFER_AMP;
	return BUFF_OK;
}

struct buffer *buffer_pool_get_buffer(struct buffer_pool *bp) {
	struct buff_pool_entry *bpe;
	int r, inuse;
	lock_pool(bp);
	if (bp->num - bp->inuse <= 0) {
		r = buffer_pool_populate_pool(bp);
		if (r == BUFF_WAIT)
			wait_for_free_buffer(bp);
	}
	/* return the first not in use buffer */
	LIST_FOREACH(bpe, bp->pool, entries) {
		if (bpe->status == STATUS_FREE) {
			bpe->status = STATUS_INUSE;
			bp->inuse++;
			unlock_pool(bp);
			return bpe->b;
		}
	}
	unlock_pool(bp);
	return NULL; /* something went wrong */
}

void buffer_pool_join_buffer(struct buffer_pool *bp, struct buffer *b) {
	struct buff_pool_entry *bpe;
	lock_pool(bp);
	LIST_FOREACH(bpe, bp->pool, entries)
		if (bpe->b == b) {
			assert(bpe->status == STATUS_INUSE);
			buffer_zero_buffer(bpe->b);
			bpe->status = STATUS_FREE;
			bp->inuse--;
			pthread_cond_signal(&bp->cond);
			unlock_pool(bp);
			return;
		}
	/* i shoundnt be here*/
	unlock_pool(bp);
	fprintf(stderr, "buffer not found in pool\n");
	abort();
}

void buffer_pool_stop_buffers(struct buffer_pool *bp) {
	struct buff_pool_entry *bpe, *bpp;
	lock_pool(bp);
	bpe = LIST_FIRST(bp->pool);
	while (bpe != NULL) {
		bpp = LIST_NEXT(bpe, entries);
		buffer_truncate_buffer(bpe->b);
		free(bpe->b);
		free(bpe);
		bpe = bpp;
	}
	unlock_pool(bp);
	LIST_INIT(bp->pool);	
	free(bp->pool);

	pthread_mutex_destroy(&bp->mutex);
	pthread_cond_destroy(&bp->cond);
	free(bp);
}
