/**
 * database.c
 *
 * SQLite database management for tracking the user's video file viewing
 * history.
 *
 * OVERVIEW:
 * Responsible for handling all database operations for logging which films have
 * been watched, when they were last watched, and how many times they have been
 * watched. I opted for SQLite because it has a fairly simple C API and does not
 * require a separate database server.
 *
 * DATABASE SCHEMA:
 * FILMS table:
 * - ID: Auto-incrementing primary key
 * - TITLE: Film title (extracted from the filename)
 * - WATCHCOUNT: Number of times watched
 * - LASTWATCHED: Timestamp of most recent viewing
 */
#include <errno.h>
#include <linux/limits.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "config.h"
#include "database.h"

/* File-static database handle */
static sqlite3 *db;

/**
 * db_cleanup - Close database connection
 *
 * This closes the database and flushes any pending writes.
 */
void db_cleanup(void) { sqlite3_close(db); }

/**
 * db_insert - Log a film viewing to the database
 * @path: FUSE path to the film ("/file.mp4")
 *
 * This records that a film was watched by inserting a new row if we haven't
 * watched it before or incrementing the watch count and updating the timestamp
 * if we have.
 *
 * Return: 0 on success, -1 on error
 */
int db_insert(const char *path) {
  char *error_msg_buffer = 0;

  /* This duplicates everything after the leading slash into a malloc'd string*/
  char *title = strdup(path + 1);
  if (!title) {
    fprintf(stderr, "Failed to duplicate path string: %s", strerror(errno));
    return -1;
  }

  /**
   * We only want the title, so we terminate the string at the last '.'
   * character.
   */
  char *extension = strrchr(title, '.');
  if (!extension) {
    fprintf(stderr, "Missing extension in filename. This should not happen.");
    free(title);
    return -1;
  }

  *extension = '\0';

  /**
   * 8192 bytes should be more than enough for our command template and even
   * long film titles.
   */
  char *sql = malloc(SQL_COMMAND_MAX);
  if (!sql) {
    fprintf(stderr, "Memory allocation failed for sql command string: %s",
            strerror(errno));
    free(title);
    return -1;
  }

  /**
   * The approach that we use here of embedding the title into the SQL string is
   * NOT SAFE because the user could inject commands through clever file
   * naming. Though this is primarily an educational project, this will likely
   * be changed to use prepared statements in the future.
   */
  snprintf(sql, SQL_COMMAND_MAX,
           "INSERT INTO FILMS (TITLE, WATCHCOUNT) "
           "VALUES ('%s', 1) "
           "ON CONFLICT(TITLE) DO UPDATE SET "
           "WATCHCOUNT = WATCHCOUNT + 1, LASTWATCHED = current_timestamp;",
           title);

  free(title);

  /* This is where the SQL statement is prepared and executed. */
  if (sqlite3_exec(db, sql, NULL, 0, &error_msg_buffer) != SQLITE_OK) {
    fprintf(stderr, "SQL Error: %s\n", error_msg_buffer);
    /*
     * SQLite has its own approach to allocating memory, so we use its function
     * to free the error_msg_buffer.
     */
    sqlite3_free(error_msg_buffer);
    error_msg_buffer = NULL;
    free(sql);
    return -1;
  }

  free(sql);
  return 0;
}

/**
 * create_table - Create the FILMS table if it doesn't exist
 *
 * This creates the database schema. Adding "IF NOT EXISTS" makes it so we could
 * run this multiple times without destroying data.
 *
 * Return: 0 on success, -1 on error
 */
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
    return -1;
  }
  return 0;
}

/**
 * db_init - Initialize database connection and create the schema
 *
 * Opens (or creates if needed) the SQLite database file and sets up the schema.
 *
 * Return: 0 on success, -1 on error
 */
int db_init(void) {
  static const int dir_path_permissions = 0700; // RWE for owner, none otherwise
  const char *home = get_config()->home;

  char *db_file_path = "/.filmfs/films.db";
  const size_t db_path_len = strlen(home) + strlen(db_file_path) + 1;
  if (db_path_len > PATH_MAX) {
    fprintf(stderr,
            "Database file path exceeds PATH_MAX, this should not happen.");
    return -1;
  }

  /* We build the path to the database file */
  char *db_path = malloc(db_path_len + 1);
  if (!db_path) {
    fprintf(stderr, "Memory allocation failed for DB path: %s",
            strerror(errno));
    return -1;
  }

  snprintf(db_path, db_path_len, "%s%s", home, db_file_path);

  char *dir_path = malloc(db_path_len);
  strncpy(dir_path, db_path, db_path_len);
  char *last_slash = strrchr(dir_path, '/');
  if (last_slash) {
    *last_slash = '\0';
  }

  struct stat buffer;
  /* We make the configuration directory if it doesn't exist */
  if (stat(dir_path, &buffer) == -1) {
    if (mkdir(dir_path, dir_path_permissions) == -1) {
      fprintf(stderr, "Failed to make config directory: %s", strerror(errno));
      free(db_path);
      free(dir_path);
      return -1;
    }
  }

  /* We open the database file or create it if it doesn't exist, then set up the
   * database connection*/
  if (sqlite3_open(db_path, &db) != SQLITE_OK) {
    fprintf(stderr, "Failed to open database: %s\n", sqlite3_errmsg(db));
    free(db_path);
    free(dir_path);
    return -1;
  }

  /* Create the FILMS table if it doesn't exist */
  if (create_table() == -1) {
    free(db_path);
    free(dir_path);
    return -1;
  }

  free(db_path);
  free(dir_path);

  return 0;
}
