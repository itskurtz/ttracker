#ifndef H_TSERVER
#define H_TSERVER
#include <pthread.h>
#if defined(__FreeBSD__)
#include <pthread_np.h>
#endif
#include <sys/queue.h>
#include "base.h"
#include "buffer_pool.h"
#include "td_pool.h"

/* thread list */
struct pth_entry {
	pthread_t	th;
	int		clientfd;
	TAILQ_ENTRY(pth_entry) entries;
};

TAILQ_HEAD(pth_list, pth_entry);

struct server {
	struct pth_list		*th_list;
	int			 th_count;	/* threads count */
	int			 fd;
	int			 tries;
	struct server_q		*sq;
	struct buffer_pool	*bp;
	struct td_pool		*tp;
};



void	*handle_req(void *ptr);
void *handle_jobs(void *ptr);
int runq_events(struct server *s);
int server_do_stats(struct server *s);
int shutdown_server(struct server *s) __attribute__((cold));
int update_max_try(struct server *s);
int set_shutdown(struct server *s, int status);
void do_shutdown(struct server *s);
struct server *build_server_connection(void) __attribute__((cold));
void server_init_server_list(struct server *s) __attribute__((cold));
void server_init_threads(struct server *s) __attribute__((cold));
void main_server(void) __attribute__((cold));
void update_server(struct server *s);
void cleanup_server(struct server *s, int force) __attribute__((cold));

#endif
