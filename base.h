#ifndef H_BASE
#define H_BASE
#include <netinet/in.h>
#include <sys/queue.h>
#include <unistd.h>
#include "buffer_pool.h"
#include "td_pool.h"

#define NORMAL_SHUTDOWN (1)
#define FORCED_SHUTDOWN (2)
#define MAX_TRY (10)
#define TIMEOUT (1)
#define MAX_THREADS (8)
#define MAX_TDATA_POOL (64)
/* sysconf(_SC_THREAD_THREADS_MAX) */

LIST_HEAD(td_list, td_entry);

struct thread_data;
struct server;
struct server_q;

struct thread_data {
	struct server_q	*sq;
	int		 client;
	struct sockaddr_in clientaddr;
	struct buffer_pool *bp;
	struct td_pool *tp;
};

struct td_list_t {
	struct td_list *td_list; /* thread data list */
	uint inuse;
	pthread_cond_t *cond;
	pthread_mutex_t *mtx;
};

struct td_entry {
	LIST_ENTRY(td_entry) entries;
	struct thread_data *td;
};

struct server_q {
	/* threads work on these */
	struct workqueue_t	*wq;
	struct torrent_list_t	*tl; /* torrent list */
	struct td_list_t	*tlist; /* thread data list */
	struct td_pool *tp;
};

#endif
