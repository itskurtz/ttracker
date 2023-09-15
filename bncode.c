#include <search.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "torrent.h"
#include "buffer.h"
#include "bncode.h"
#include "mem.h"

int encode_bytes (const char *input, const int len, char *output, int out_size) {
	int ret;
	if (len <= 0 || output == NULL) {
		return -1;
	}
	ret = snprintf(output, out_size, "%d:%s", len, input);
	/* if (ret >= out_size) { */
	/* 	printf("bnecode bytes input too big\n"); */
	/* 	exit(1); */
	/* } */
	return strlen(output);
}

int encode_int (const int input, char *output, int out_size) {
	if (output == NULL) {
		return -1;
	}
	snprintf(output, out_size, "i%de", input);
	return strlen(output);
}

struct benc_dict *mk_benc_dict(void) {
	struct benc_dict *pdict;
	pdict = xmalloc(sizeof(struct benc_dict));
	LIST_INIT(pdict);
	return pdict;
}
struct benc_list_entry *mk_benc_list_entry(void) {
	struct benc_list_entry *ple;
	ple   = xmalloc(sizeof(struct benc_list_entry));
	return ple;
}

void benc_list_entry_set_v(struct benc_list_entry *ple, void *data, int type) {
	ple->data = data;
	ple->type = type;
}


/* nocheck whatsoever on key */
struct benc_dict_entry *mk_benc_dict_entry_int(char *key, long val) {
	struct benc_dict_entry *pe;
	
	pe	 = xmalloc(sizeof(*pe));
	pe->key	 = xmalloc(sizeof(char) *	PEER_HASH_LEN+1);
	pe->data = (void *)val;
	pe->type = T_INT;

	strncpy(pe->key, key, PEER_HASH_LEN);
	return pe;
}

struct benc_dict_entry *mk_benc_dict_entry_string(char *key, char *val) {
	struct benc_dict_entry *pe;
	pe	 = xmalloc(sizeof(*pe));
	pe->key	 = xmalloc(sizeof(char) *	(PEER_HASH_LEN+1));
	pe->data = xmalloc(sizeof(char) *	(PEER_HASH_LEN+1));
	pe->type = T_BYTESTRING;
		
	strncpy(pe->key, key, PEER_HASH_LEN);
	strncpy(pe->data, val, PEER_HASH_LEN);

	return pe;
}

struct benc_dict_entry *mk_benc_dict_entry_kv(char *key, void *val, int type) {
	struct benc_dict_entry *pe;
	pe	 = xmalloc(sizeof(*pe));
	pe->key	 = xmalloc(sizeof(char) *	(PEER_HASH_LEN+1));
	pe->data = val;
	pe->type = type;

	memcpy(pe->key, key, PEER_HASH_LEN);
	return pe;
}

void bencode_init_peer(struct peer_entry *pt, struct benc_dict *dict) {
	/* keep the order */
	struct benc_dict_entry *pe;

	pe = mk_benc_dict_entry_int("port", pt->port);
	LIST_INSERT_HEAD(dict, pe, entries);

	pe = mk_benc_dict_entry_string("peer_id", pt->peer_id);
	LIST_INSERT_HEAD(dict, pe, entries);
	
	pe = mk_benc_dict_entry_string("ip", pt->ip);
	LIST_INSERT_HEAD(dict, pe, entries);
}

/* recursive freeing */
void bencode_free_with_keys(void *data, int type) {
	struct benc_list_entry *ple, *nple;
	struct benc_dict_entry *pde, *npde;
	switch (type) {
	case T_INT:
		break;
	case T_BYTESTRING:
		free(data);
		break;
	case T_LIST:
		ple = LIST_FIRST((struct benc_list *)data);
		while (ple != NULL) {
			nple = LIST_NEXT(ple, entries);
			bencode_free_with_keys(ple->data, ple->type);
			free(ple);
			ple = nple;
		}
		LIST_INIT((struct benc_dict *)data);
		free(data);
		break;
	case T_DICT:
		pde = LIST_FIRST((struct benc_dict *)data);
		while (pde != NULL) {
			npde = LIST_NEXT(pde, entries);
			bencode_free_with_keys(pde->key, T_BYTESTRING);
			bencode_free_with_keys(pde->data, pde->type);
			free(pde);
			pde = npde;
		}
		LIST_INIT((struct benc_dict *)data);
		free(data);
		break;
	default:
		/* default take a good look before falling here */
		free(data);
		break;
	}
}

/* not the fastes but does the job */
int bencode(void *data, int type, char *out, int out_size) {

	struct benc_list_entry	*pt;
	struct benc_dict_entry	*pd;
	int	 count = 0;
	switch (type) {
	case T_INT:
		return encode_int((long)data, out, out_size);
	case T_BYTESTRING:
		return encode_bytes((char *)data, strlen((char *)data), out, out_size);
	case T_LIST:
		count = snprintf(out, out_size-count, "l");
		LIST_FOREACH(pt, (struct benc_list *)data, entries)
			count += bencode(pt->data, pt->type, out+count, out_size-count);
		count += snprintf(out+count, out_size-count, "e");
		return count;
	case T_DICT:
		count = snprintf(out, out_size-count, "d");
		LIST_FOREACH(pd, (struct benc_dict *)data, entries) {
			count += bencode(pd->key, T_BYTESTRING, out+count, out_size-count);
			count += bencode(pd->data, pd->type, out+count, out_size-count);
		}
		count += snprintf(out+count, out_size-count, "e");
		return count;
	default:
		fprintf(stderr, "nothing to encode\n");
		return 0;
	}
	return 0;
}
