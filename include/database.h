#ifndef DB_H
#define DB_H

#include "common.h"
#include <sqlite3.h>

#define PATH_MAX 4096
#define SQL_COMMAND_MAX 8192

int db_init(void);
void db_cleanup(void);
int db_insert(const char *path);

#endif
