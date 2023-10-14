#ifndef H_HTTP
#define H_HTTP
#include <sys/queue.h>
#include "base.h"
#include "buffer_pool.h"
#include "torrent_ctx.h"
#include "work.h"
#define HTTP_MAX_REQ_SIZE (2048)
#define HTTP_MAX_KEYS (6)
#define HTTP_MAX_KEY_LEN (128)
#define HTTP_MAX_VAL_LEN (256)
#define HTTP_200 "HTTP/1.1 200 OK"
#define HTTP_400 "HTTP/1.1 400 Bad Request"
#define HTTP_500 "HTTP/1.1 500 Internal Server Error"
/* errors regarding peer requests */
#define E_INFO_HASH	(0x01)
#define E_PEER_ID	(0x02)
#define E_PORT		(0x03)
#define E_DOWNLOADED	(0x04)
#define E_UPLOADED	(0x05)
#define E_LEFT		(0x06)
#define E_EVENT		(0x07)
#define E_DEFAULT	(0xff)

struct http_uri_kv {
	char key[HTTP_MAX_KEY_LEN];
	char val[HTTP_MAX_VAL_LEN];
};

struct http_kv_entry {
	struct http_uri_kv data;
	TAILQ_ENTRY(http_kv_entry) entries;
};

TAILQ_HEAD(http_kv_list, http_kv_entry);	

int http_handle_read(struct thread_data *td, struct buffer *b);
int http_handle_write(struct job_t *job);
void http_error(struct thread_data *td, int client, int code);
char *http_header(size_t content_len);
int http_response(struct thread_data *td, int fdclient, struct buffer *b, int code);
int http_parse_query(struct buffer *input, struct http_kv_list *head, int job_t);
int http_parse_job_from_request(struct buffer *input);
struct peer_req *http_build_peer_req(struct http_kv_list *list);
/* peer_req_hash_list must be used in a specific case, its not genporp..  */
struct peer_req *http_build_peer_req_hash_list(struct http_kv_list *list);
struct http_kv_entry *http_lookup_kv_key(struct http_kv_list *list, char *key);
void get_peer_info(struct sockaddr_in *caddr, char *out);
int sanitize_peer_req(struct peer_req *ctx, int *error);
#endif
