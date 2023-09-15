#include <sqlite3.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include "db.h"

static sqlite3 *G_DB = NULL;

int db_connect(const char *path) {	
	sqlite3 *db;
	int	 retcode;
	retcode = sqlite3_open(path, &db);
	if (retcode == SQLITE_OK) {
		G_DB = db;
		return 0;
	}
	return -1;
}

int db_disconnect() {
	if (G_DB) {
		sqlite3_close(G_DB);
		return 0;
	}
	return -1;
}

int db_query_lookup_torrent(const char *info_hash) {
	char query[256];
	sqlite3_stmt *stmt;
	int retcode, tid, maxtry;
	sprintf(query, "select id from torrents where lower(info_hash) = lower('%s')", info_hash);
	maxtry = 10;		/* hardcoded w/e */
	tid    = -1;
	if (sqlite3_prepare_v2(
		    G_DB, query, strlen(query), &stmt, NULL) == SQLITE_OK) {
		while ((retcode = sqlite3_step(stmt)) != SQLITE_DONE
		       && retcode != SQLITE_ERROR && maxtry > 0) {
			if (retcode == SQLITE_BUSY) {
				/* this should not block the application *\/ */
				maxtry--;
				continue;
			}
			if (retcode == SQLITE_ROW)
				tid = sqlite3_column_int(stmt, 0);
		}
	}
	if (sqlite3_finalize(stmt) != SQLITE_OK)
		fprintf(stderr, "Error finalizing prepared stmt\n");
	if (tid == -1)
		return -1;
	return tid;
}
