#include "database.h"
#include "config.h"

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
  if (!extension) {
    fprintf(stderr, "Missing extension in filename. This should not happen.");
    free(title);
    return 1;
  }

  *extension = '\0';

  char *sql = malloc(SQL_COMMAND_MAX);
  if (!sql) {
    fprintf(stderr, "Memory allocation failed for sql command string: %s",
            strerror(errno));
    free(title);
    return 1;
  }

  snprintf(sql, SQL_COMMAND_MAX,
           "INSERT INTO FILMS (TITLE, WATCHCOUNT) "
           "VALUES ('%s', 1) "
           "ON CONFLICT(TITLE) DO UPDATE SET "
           "WATCHCOUNT = WATCHCOUNT + 1, LASTWATCHED = current_timestamp;",
           title);

  free(title);

  if (sqlite3_exec(db, sql, NULL, 0, &error_msg_buffer) != SQLITE_OK) {
    fprintf(stderr, "SQL Error: %s\n", error_msg_buffer);
    sqlite3_free(error_msg_buffer);
    error_msg_buffer = NULL;
    free(sql);
    return 1;
  }

  free(sql);
  return 0;
}

int create_table(void) {
  char *sql = "CREATE TABLE IF NOT EXISTS FILMS("
              "ID INTEGER PRIMARY KEY AUTOINCREMENT,"
              "TITLE  TEXT NOT NULL UNIQUE,"
              "WATCHCOUNT INT NOT NULL,"
              "LASTWATCHED TEXT NOT NULL DEFAULT current_timestamp);";

  char *error_msg_buffer = 0;

  if (sqlite3_exec(db, sql, NULL, 0, &error_msg_buffer) != SQLITE_OK) {
    fprintf(stderr, "SQL Error: %s\n", error_msg_buffer);
    sqlite3_free(error_msg_buffer);
    error_msg_buffer = NULL;
    return 1;
  }
  return 0;
}

int db_init(void) {
  static const int dir_path_permissions = 0700; // RWE for owner, none otherwise
  const char *home = get_config()->home;

  char *db_file_path = "/.filmfs/films.db";
  const size_t db_path_len = strlen(home) + strlen(db_file_path) + 1;
  if (db_path_len > PATH_MAX) {
    fprintf(stderr,
            "Database file path exceeds PATH_MAX, this should not happen.");
    return 1;
  }

  char *db_path = malloc(db_path_len + 1);
  if (!db_path) {
    fprintf(stderr, "Memory allocation failed for DB path: %s",
            strerror(errno));
    return 1;
  }

  snprintf(db_path, db_path_len, "%s%s", home, db_file_path);

  char *dir_path = malloc(db_path_len);
  strncpy(dir_path, db_path, db_path_len);
  char *last_slash = strrchr(dir_path, '/');
  if (last_slash) {
    *last_slash = '\0';
  }

  struct stat buffer;
  if (stat(dir_path, &buffer) == -1) {
    if (mkdir(dir_path, dir_path_permissions) == -1) {
      fprintf(stderr, "Failed to make config directory: %s", strerror(errno));
      free(db_path);
      free(dir_path);
      return 1;
    }
  }

  if (sqlite3_open(db_path, &db) == 1) {
    fprintf(stderr, "Failed to open database: %s\n", sqlite3_errmsg(db));
    free(db_path);
    free(dir_path);
    return 1;
  }

  if (create_table() == 1) {
    free(db_path);
    free(dir_path);
    return 1;
  }

  free(db_path);
  free(dir_path);

  return 0;
}
