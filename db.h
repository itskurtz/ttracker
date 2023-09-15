#ifndef H_DBSQL3
#define H_DBSQL3
int db_query_lookup_torrent(const char* info_hash);
int db_connect(const char *path); 
int db_disconnect() __attribute__((cold));
#endif
