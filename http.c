#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <curl/curl.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "http.h"
#include "torrent.h"
#include "server.h"
#include "mem.h"

static void url_decode(char *out, char *in);

struct http_kv_entry *http_lookup_kv_key(struct http_kv_list *list, char *key) {
	struct http_kv_entry *np;
	TAILQ_FOREACH(np, list, entries)
		if (strcmp(np->data.key, key) == 0)
			return np;
	return NULL;
}

struct peer_req *http_build_peer_req_hash_list(struct http_kv_list *list) {
	struct peer_req		*ctx;
	struct http_kv_entry	*np;
	struct hashentry_t *pp;

	ctx = xmalloc(sizeof(*ctx));
	ctx->hashlist = xmalloc(sizeof(struct hashlist_t));
	LIST_INIT(ctx->hashlist);
	TAILQ_FOREACH(np, list, entries) {
		pp = xmalloc(sizeof(struct hashentry_t));
		url_decode(pp->info_hash, np->data.val);
		LIST_INSERT_HEAD(ctx->hashlist, pp, entries);
	}
	return ctx;
}

/* everything must be sanitized */
struct peer_req *http_build_peer_req(struct http_kv_list *list) {
	struct http_kv_entry *np;
	struct peer_req *ctx = xmalloc(sizeof(struct peer_req));

	memset(ctx, 0, sizeof(struct peer_req));
	np = http_lookup_kv_key(list, "info_hash");
	/* first decode the url */
	
	url_decode(ctx->info_hash, np->data.val);
	/* strcpy(ctx->info_hash, np->data.val); */
	
	np = http_lookup_kv_key(list, "peer_id");
	strcpy(ctx->peer_id, np->data.val);
	
	np = http_lookup_kv_key(list, "port");
	ctx->port = atoi(np->data.val);
	
	np = http_lookup_kv_key(list, "downloaded");
	ctx->downloaded = atoi(np->data.val);
	
	np = http_lookup_kv_key(list, "uploaded");
	ctx->uploaded = atoi(np->data.val);

	np = http_lookup_kv_key(list, "left");
	ctx->left = atoi(np->data.val);
	/* np = http_lookup_kv_key(list, "compact"); */
	/* ctx->compact = atoi(np->data.val); */
	
	np = http_lookup_kv_key(list, "event");
	if (np == NULL) {
		strcpy(ctx->event, "count");
	}
	else
		strcpy(ctx->event, np->data.val);
	ctx->hashlist = NULL;
	return ctx;
}

/* I should use a static table or similar, this is too expensive */
static void url_decode(char *out, char *in) {
	CURL *curl = curl_easy_init();
	int i;
	if(curl) {
		int decodelen;
		unsigned char *decoded = (unsigned char *)curl_easy_unescape(curl, in, strlen(in), &decodelen);
		if(decoded) {
			if (decodelen < 20) {
				curl_free(decoded);
				curl_easy_cleanup(curl);
				return;
			}
			for (i = 0; i < 20; ++i)
				sprintf(out+i*2, "%02x", decoded[i]);
			curl_free(decoded);
		}
		curl_easy_cleanup(curl);
	}
}

/* nothing fancy just an ez write */
int http_handle_write(struct job_t *job) {
	/* for now its error code 200 we'll se what else is needed */  
	return http_response(job->thread_data, job->thread_data->client, job->out, 200);
}

int http_handle_read(struct thread_data *td, struct buffer *b) {
	/* if there are lots of requests keep in mind memory consumption
	   eg. 30 requests * 2048 each one i have to limit it
	   using some kind of memory management strategy!
	   or... or... firewall, do your job!
	*/
	int len, cur, error;
		/* wip */
	struct http_kv_list	*http_kv_list;
	struct http_kv_entry	*keyval, *np;
	struct job_t *newjob;
	int job_t;
	// char *resp_bencoded = (char *)malloc(sizeof(char) * 2048);
	/* +1  \0 */
	char *uri = (char *)xmalloc(sizeof(char) * (HTTP_MAX_REQ_SIZE+1));
	http_kv_list = (struct http_kv_list *)xmalloc(sizeof(struct http_kv_list));
	/* same thing as calloc */
	memset(uri, 0, sizeof(char) * (HTTP_MAX_REQ_SIZE+1));
	memset(uri, 0, sizeof(struct http_kv_list));
	error = 0;
	len = read(td->client, uri, HTTP_MAX_REQ_SIZE);
	if (len == -1 || len < 5) {
		error = -1;
		/* error handler */
		free(uri);
		free(http_kv_list);
		return error;
	}
	buffer_append_string_to_buffer(b, uri);
	
	TAILQ_INIT(http_kv_list);
	if ((job_t = http_parse_job_from_request(b)) == -1) {
		free(http_kv_list);
		free(uri);
		error = -1;
		/* fprintf(stderr, "no job found"); */
		return error;
	}
	if (http_parse_query(b, http_kv_list, job_t) == -1) {
		/* fprintf(stderr, "Error query parsing\n"); */
		http_error(td, td->client, 400);
		/* this must be fixed */
		free(http_kv_list);
		error = -1;
	}
	else {
		buffer_zero_buffer(b);
		newjob = mk_job();
		newjob->type = job_t;
		newjob->out = buffer_pool_get_buffer(td->bp);
		/* building new job */
		newjob->thread_data  = td;
		newjob->http_kv_list = http_kv_list;
		
		/* once done I can enqueue it */
		workq_enqueue_job(td->sq->wq, newjob);
		error = 0;
	}
	
	free(uri);
	return error;
}

/* get peer's ip should do the job */
void get_peer_info(struct sockaddr_in *caddr, char *out) {
	char ip[INET_ADDRSTRLEN];
	struct in_addr ipaddr = ((struct sockaddr_in *)caddr)->sin_addr;
	inet_ntop(AF_INET, &ipaddr, ip, INET_ADDRSTRLEN);
	strncpy(out, ip, INET_ADDRSTRLEN);
}


void http_error(struct thread_data *td, int client, int code) {
	struct buffer *b;
	b = buffer_pool_get_buffer(td->bp);
	buffer_append_string_to_buffer(b, "d14:failure reason11:query errore");
	http_response(td, client, b, code);
	buffer_pool_join_buffer(td->bp, b);
}

int http_response(struct thread_data *td, int client, struct buffer *body, int code) {
	struct buffer	*b;
	int		 b_len, written, outsiz, ret, max_try;
	char		 closing[64];
	memset(closing, 0, sizeof(char) * 64);

	b = buffer_pool_get_buffer(td->bp);
	switch (code) {
	case 200:
		buffer_append_string_to_buffer(b, HTTP_200);
		break;
	case 400:
		buffer_append_string_to_buffer(b, HTTP_400);
		break;
	default:
		/* http 500 */
		buffer_append_string_to_buffer(b, HTTP_500);
		break;
	}

	b_len = buffer_len(body);

	/* close http header */
	if (b_len > 0) {
		snprintf(closing, 64, "\ncontent-length: %d\n", b_len);
		buffer_append_string_to_buffer(b, closing);
		buffer_append_string_to_buffer(b, "content-type: text/plain\n");
		buffer_append_string_to_buffer(b, "server: topkek-0.1\n");
		buffer_append_string_to_buffer(b, "connection: close\n");
	}
	buffer_append_string_to_buffer(b, "\r\n");
	buffer_append_to_buffer(b, body);

	outsiz	= b->cursor;
	written = 0;
	max_try = 5;
	do {
		ret = write(client, (b->p)+written, outsiz-written);
		if (ret == -1) { /* sigpipe most of the times */
			sleep(1);
			max_try--;

			if (max_try == 0) {
			  	buffer_pool_join_buffer(td->bp, b);
				perror("write: ");
				return -1;
			}
			ret = 0;
		}
		written += ret;
	} while (written < outsiz);
	buffer_pool_join_buffer(td->bp, b);
	return written;
}

int http_parse_job_from_request(struct buffer *input) {
	if (strncmp(input->p, "GET /announce", 13) == 0)
		return JOB_T_ANNOUNCE;
	if (strncmp(input->p, "GET /scrape", 11) == 0)
		return JOB_T_SCRAPE;
	return -1;
}

/* thats just boring */
int http_parse_query(struct buffer *input, struct http_kv_list *head, int job_t) {
	/* necessary keys */

	/* need the url */
	int i, k, found, blen;
	char key[HTTP_MAX_KEY_LEN];
	char val[HTTP_MAX_VAL_LEN];
	struct http_kv_entry *keyval, *np;
	const char *keys[] = {"info_hash", "peer_id",
			      /* "ip", */ "port", "uploaded",
			      "downloaded", "left"};
	i = k = 0;
	/* look for a match in keys*/
	blen = buffer_len(input);
	for (i = 0, found = 0; i < blen; i++) {
		if (input->p[i] == '\n') {
			if (input->p[i-1] == '\r')
				found = i;
		}
		if (found)
			break;
	}
	if (!found) {
		fprintf(stderr, "No query provided\n.. i got %s", input->p);
		return -1;
	}
	if (found < 11) {
		/* atleast scrape */
		fprintf(stderr, "Wrong query provided\n.. i got %s", input->p);
		return -1;
	}
	if (strncmp(&(input->p[i-9]), "HTTP/1.1", 8) != 0 && strncmp(&(input->p[i-9]), "HTTP/1.0", 8) != 0) {
		fprintf(stderr, "%s\n", &(input->p[i-9]));
		return -1;
	}
	/* ugly af */
	input->p[i-10] = '\0';
	input->cursor -= 11;
	
	i = 0;
	
	if (job_t == JOB_T_ANNOUNCE)
		i += 14;
	else if (job_t == JOB_T_SCRAPE)
		i += 12;
	else {
		fprintf(stderr, "req: %s\n", input->p);
		return -1;
	}
	
	while (i < found) {
		for (k = 0; k < HTTP_MAX_KEY_LEN-1 && i+k < found; k++) {
			if (input->p[i+k] == '=')
				break;
			key[k] = input->p[i+k];
		}
		key[k] = '\0';
		if (k == 0) {
			/* fprintf(stderr, "I have 0 keys %s\n", input->p+i); */
			return -1;
		}
		i = i+k+1;
		for (k = 0; k < HTTP_MAX_VAL_LEN-1 && i+k < found; k++) {
			if (input->p[i+k] == '&')
				break;
			val[k] = input->p[i+k];
		}
		val[k] = '\0';
		i = i+k+1;
		
		keyval = xmalloc(sizeof(struct http_kv_entry));

		strncpy(keyval->data.key, key, HTTP_MAX_KEY_LEN);
		strncpy(keyval->data.val, val, HTTP_MAX_VAL_LEN);
		
		TAILQ_INSERT_HEAD(head, keyval, entries);
	}

	if (job_t == JOB_T_ANNOUNCE) {
		/* I want all the keys listed before if announce */
		for (i = 0, found = 0; i < HTTP_MAX_KEYS; i++)
			TAILQ_FOREACH(np, head, entries)
				if (strcmp(keys[i], np->data.key) == 0)
					found += 1;
	
		if (found < HTTP_MAX_KEYS) {
			fprintf(stderr, "not all keys ar present, aborting\n");
			np = TAILQ_FIRST(head);
			while (np != NULL) {
				keyval = TAILQ_NEXT(np, entries);
				free(np);
				np = keyval;
			}
			printf("req: %s\n", input->p);
			return -1;
		}
		return 0;
	}
	else if (job_t == JOB_T_SCRAPE) {
		TAILQ_FOREACH(np, head, entries)
			if (strcmp("info_hash", np->data.key) != 0) {
				fprintf(stderr, "i want info_hash i got %s\n", np->data.key);
				fprintf(stderr, "req: %s\n", input->p);
				return -1;
			}
		/* ok for now*/
		return 0;
	}

	/* validate every kv */
	return -1;
}
