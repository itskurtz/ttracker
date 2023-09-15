#ifndef H_TORRENT_CTX
#define H_TORRENT_CTX

#define TORRENT_HASH_LEN (256)
#define PEER_HASH_LEN (64)
#define PEER_EVENT_MAX (32)

struct torrent_ctx {
	int		tid;

	char		info_hash[TORRENT_HASH_LEN+1];
	int		free_torrent;
	int		peer_count;
	int		seed_count;
	int		down_count;
	
};


#endif
