#include "database.h"

static sqlite3 *db;

void db_cleanup(void) { sqlite3_close(db); }

int db_insert(const char *path) {
  char *error_msg_buffer = 0;

  char *title = strdup(path + 1); // Skip leading slash
  if (!title) {
    fprintf(stderr, "Failed to duplicate path string: %s", strerror(errno));
    return 1;
  }

  char *extension = strrchr(title, '.');
  *extension = '\0';

  char *sql = malloc(SQL_COMMAND_MAX);
  if (!sql) {
    fprintf(stderr, "Memory allocation failed for sql command string: %s",
            strerror(errno));
    return 1;
  }
  
  snprintf(sql, SQL_COMMAND_MAX,
           "INSERT INTO COMPANY (TITLE, WATCHCOUNT) "
           "VALUES ('%s', 1) "
           "ON CONFLICT(TITLE) DO UPDATE SET "
           "WATCHCOUNT = WATCHCOUNT + 1, LASTWATCHED = current_timestamp;",
           title);

  free(title);

  if (sqlite3_exec(db, sql, NULL, 0, &error_msg_buffer) != SQLITE_OK) {
    fprintf(stderr, "SQL Error: %s\n", error_msg_buffer);
    sqlite3_free(error_msg_buffer);
    free(sql);
    return 1;
  }

  free(sql);
  return 0;
}

int create_table(void) {
  char *sql = "CREATE TABLE IF NOT EXISTS COMPANY("
              "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
              "TITLE  TEXT NOT NULL UNIQUE,"
              "WATCHCOUNT INT NOT NULL,"
              "LASTWATCHED TEXT NOT NULL DEFAULT current_timestamp);";

  char *error_msg_buffer = 0;

  if (sqlite3_exec(db, sql, NULL, 0, &error_msg_buffer) != SQLITE_OK) {
    fprintf(stderr, "SQL Error: %s\n", error_msg_buffer);
    sqlite3_free(error_msg_buffer);
    return 1;
  }
  return 0;
}

int db_init(void) {
  if (sqlite3_open("test.db", &db) == 1) {
    fprintf(stderr, "Failed to open database: %s\n", sqlite3_errmsg(db));
    return 1;
  }

  if (create_table() == 1) {
    return 1;
  }

  return 0;
}
