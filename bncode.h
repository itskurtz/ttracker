#ifndef H_BNCODE
#define H_BNCODE
#include <sys/queue.h>
#include "torrent.h"

#define T_BYTESTRING (0)
#define T_INT (1)
#define T_LIST (2)
#define T_DICT (3)
#define T_BYTE (4)

LIST_HEAD(benc_list, benc_list_entry);
LIST_HEAD(benc_dict, benc_dict_entry);

struct benc_list_entry {
	LIST_ENTRY(benc_list_entry) entries;
	void *data;
	int type;
};

struct benc_dict_entry {
	LIST_ENTRY(benc_dict_entry) entries;
	void *data;
	char *key;
	int type;
};

int encode_bytes (const char *input, const int len, char *output, int out_size);
int encode_int (const int input, char *output, int out_size);

void benc_list_entry_set_v(struct benc_list_entry *ple, void *data, int type);

struct benc_dict *mk_benc_dict(void);
struct benc_list_entry *mk_benc_list_entry(void);
struct benc_dict_entry *mk_benc_dict_entry_kv(char *key, void *val, int type);
struct benc_dict_entry *mk_benc_dict_entry_string(char *key, char *val);
struct benc_dict_entry *mk_benc_dict_entry_int(char *key, long val);

/* it does not quite fit here */
void bencode_init_peer(struct peer_entry *pt, struct benc_dict *dict);
void bencode_free_with_keys(void *data, int type);
int bencode(void *data, int type, char *out, int out_size);
#endif
