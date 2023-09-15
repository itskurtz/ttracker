#ifndef H_BUFFPOOL
#define H_BUFFPOOL
#include <assert.h>
#include <pthread.h>
#include <sys/queue.h>
#include "mem.h"
#include "buffer.h"
#define STATUS_FREE (0)
#define STATUS_INUSE (1)
#define MAX_BUFFERS (64)
#define BUFFER_AMP (4)

#define BUFF_WAIT (1)
#define BUFF_OK (0)

LIST_HEAD(buff_pool_list, buff_pool_entry);

struct buffer_pool {
	struct buff_pool_list	*pool;
	int			 inuse;
	int			 num;
	pthread_mutex_t		 mutex;
	pthread_cond_t		 cond;
};

struct buff_pool_entry {
	LIST_ENTRY(buff_pool_entry) entries;
	struct buffer	*b;
	int		 status;
};

void buffer_pool_init(struct buffer_pool *bp);
int buffer_pool_populate_pool(struct buffer_pool *bp);
struct buffer *buffer_pool_get_buffer(struct buffer_pool *bp);
void buffer_pool_join_buffer(struct buffer_pool *bp, struct buffer *b);
void buffer_pool_stop_buffers(struct buffer_pool *bp);
#endif
