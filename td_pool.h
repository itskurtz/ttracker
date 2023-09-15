#ifndef H_TD_POOL
#define H_TD_POOL
#include <pthread.h>
#include <sys/queue.h>
#include "base.h"

/* exact copy of thread data and buffer pool data */

#define TD_POOL_OK (0)
#define TD_POOL_WAIT (1)
#define TD_POOL_MAX_TD (16) /* double the threads*/
#define TD_POOL_STATUS_FREE (0)
#define TD_POOL_STATUS_INUSE (1)
#define TD_POOL_AMP (4)


LIST_HEAD(td_pool_list, td_pool_entry);

struct td_pool {
	struct td_pool_list	*pool;
	int			 inuse;
	int			 num;
	pthread_mutex_t		 mutex;
	pthread_cond_t		 cond;
};

struct td_pool_entry {
	LIST_ENTRY(td_pool_entry) entries;
	struct thread_data	*thread_data;
	int			 status;
};

void td_pool_init(struct td_pool *td_pool);
int td_pool_populate_pool(struct td_pool *td_pool);
struct thread_data *td_pool_get_td(struct td_pool *tdp);
void td_pool_join_td(struct td_pool *tdp, struct thread_data *td);
void td_stop_td_pool(struct td_pool *tdp);
#endif
