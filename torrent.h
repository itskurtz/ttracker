#ifndef H_TORRENT
#define H_TORRENT
#include <pthread.h>
#include <sys/queue.h>
#include "torrent_ctx.h"
#include "buffer.h"

#define T_MAX_NUMWANT (80)
enum {T_NULL, T_START, T_STOP, T_COMPLETE};

LIST_HEAD(torrent_list, torrent_entry);
LIST_HEAD(peer_list, peer_entry);
LIST_HEAD(hashlist_t, hashentry_t);
LIST_HEAD(scrapelist_t, scrapeentry_t);

struct peer_req {
	// char		tid;
	/* what the peer is looking for*/
	/* if these are negative i'm gonna fk u up*/
	unsigned long		 downloaded;
	unsigned long		 uploaded;
	char			 peer_id[PEER_HASH_LEN+1];
	char			 last_event[PEER_EVENT_MAX+1];
	char			 info_hash[TORRENT_HASH_LEN+1];
	char			 event[PEER_EVENT_MAX+1];
  	int			 port;
	int			 compact;
	long			 left;
	struct hashlist_t	*hashlist;
};

struct torrent_list_t {
	pthread_cond_t		*cond;
	pthread_mutex_t		*mutex;
	struct torrent_list	*list;
	
	/* how many torrents are working on the list rn */
	int			 working;
};

struct hashentry_t {
	LIST_ENTRY(hashentry_t) entries;
	char info_hash[TORRENT_HASH_LEN+1];
};

struct scrape_ctx {
	struct scrapelist_t list;
	int size;
};

struct scrapeentry_t {
	int	seeders;
	int	leechers;
	int	complete;
	int	peer_count;
	char	info_hash[TORRENT_HASH_LEN];
	LIST_ENTRY(scrapeentry_t) entries;
};


struct peer_entry {
//	int	id;
	char    peer_id[PEER_HASH_LEN];
	long	port;
	char	ip[16];		// ip len
	long	left;
	/* here u can add some fields which are optional */
	
	LIST_ENTRY(peer_entry) entries;
};

struct torrent_entry {

	/* lock the peers (for now its ok) */
	pthread_rwlock_t	*peer_rwlk;
  
	struct peer_list	*peers;

	char	info_hash[TORRENT_HASH_LEN];
	int	id;
	int	seeders;
	int	leechers;
	int	complete;
	int	peer_count;
	int	free_torrent;
	
	LIST_ENTRY(torrent_entry) entries;
};



// struct torrent_ctx *torrent_build_context(struct http_kv_list *list);
struct torrent_entry *torrent_mk_torrent(void);
// void torrent_set_peer_ctx(struct torrent_entry *t, struct peer_req *ctx);
void torrent_add_torrent(struct torrent_list *head, struct torrent_entry *t);
struct torrent_entry *torrent_lookup_torrent(char *info_hash, struct torrent_list_t *tl);
struct torrent_entry *torrent_do_lookup_torrent_info_hash(char *info_hash, struct torrent_list *tl);
void torrent_msg_failure_missing_hash(struct buffer *b);
void torrent_init_cache_pool(void);
void torrent_stop_pool(struct torrent_list *list);
struct torrent_entry *torrent_search_torrent(struct torrent_entry *te,
					     struct torrent_list *torrent_l);
char *torrent_add_peer_and_return_list(struct torrent_entry *te, struct peer_entry *pe);
struct peer_entry *search_peer_by_peer_id(char *peerid);
struct peer_entry *peer_search_peer(struct peer_entry *pe, struct peer_list *peer_l);
void torrent_torrent_list_peers(struct torrent_entry *te,
				struct torrent_list *tl);
void torrent_insert_new(struct torrent_entry *te, struct peer_entry *pe);
void torrent_do_insert_new(struct torrent_entry *te,	struct peer_entry *pe,
		      struct torrent_list *torrent_l, struct peer_list *peer_l);

struct peer_entry *mk_peer(void);
struct torrent_entry *mk_torrent(void);
void torrent_print_peers(struct torrent_entry *te);
void torrent_bencode_torrent(struct torrent_entry *t, char *out, int out_size);
void torrent_msg_replay_peers(struct buffer *b, char *reply);
void torrent_remove_peer( struct torrent_entry *te, struct peer_entry *pe);
void free_peer(struct peer_entry *pe);
char *torrent_update_peer_and_return_list(struct torrent_entry *te, struct peer_entry *pe, int action);
void torrent_list_jobdone(struct torrent_list_t *list);
void torrent_list_workon(struct torrent_list_t *list);
void free_torrent(struct torrent_entry *te);


/* scrape */
int bencode_scrape_ctx(struct scrape_ctx *ctx, char *out);
void scrape_stop_ctx(struct scrape_ctx *ctx);
void scrape_add_torrent(struct scrape_ctx *ctx, struct torrent_entry *te);
struct scrape_ctx *scrape_start_ctx();

#endif
