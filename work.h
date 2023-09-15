#ifndef H_WORKQ
#define H_WORKQ
#include <sys/queue.h>
#include <pthread.h>
#include <sys/queue.h>
#include "base.h"

#define JOB_T_ANNOUNCE (0)
#define JOB_T_SCRAPE (1)
#define JOB_T_STOP (-1)

#define TD_MAX_QUEUE (32)

TAILQ_HEAD(workqueue, work_entry);

struct job_t {
	/* client file descriptor */
	struct thread_data *thread_data;
	
	/* http handle */
	struct http_kv_list	*http_kv_list;
	
	/* announce/scrape */
	int			 type;
	
	/* response type */
	int			 http_status;
	
	/* peer handle */
	/* http request which has been parsed*/
	struct peer_req *peer_req;
	
	/*internal peer structure  */
	struct peer_entry *peer;
	
	/* fields... */
	struct buffer *out;
};


struct workqueue_t {
	/* i dont actually need a counter
	   for how many current jobs i have */
	pthread_mutex_t *mtx;
	pthread_cond_t	*cond;
	struct workqueue *wq;
};

struct work_entry {
	struct job_t	*job;
	int		 id;
	int		 status;
	/* stats elements / timers etc */
	TAILQ_ENTRY(work_entry) entries;
};

/*  */
void workq_enqueue_job(struct workqueue_t *wqueue, struct job_t *job);
struct job_t *workq_dequeue_job(struct workqueue_t *wqueue);
int work_process_job(struct job_t *job);
void job_complete(struct job_t *job);
struct job_t *mk_job(void);
int workq_wait(struct server_q *sq);
void workq_wakeup(struct server_q *sq);
struct job_t * workq_wait_and_deq_job(struct server_q *sq);
struct job_t *workq_unsafe_deq_job(struct workqueue_t *wq);
int workq_safepeek(struct workqueue_t *wq);
struct thread_data *mk_td();
void td_enqueue(struct td_list_t *tlist, struct thread_data *td);
void td_wakeup(struct td_list_t *tlist);
struct thread_data *td_list_unsafe_deq_td(struct td_list *tdlist);
struct thread_data *td_wait_and_dequeue(struct td_list_t *tlist);
int td_null(struct thread_data *td);
void td_stop_list(struct td_list_t *tlist);
#endif
