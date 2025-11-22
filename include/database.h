/**
 * database.h
 *
 * Responsible for handling database initialization, management, and resource
 * cleanup.
 */

#ifndef DATABASE_H
#define DATABASE_H

/**
 * 8192 bytes should be more than enough for our command template and even
 * long film titles.
 */
#define SQL_COMMAND_MAX 8192

/* We specify the FUSE version because the API differs per version*/
#define FUSE_USE_VERSION 30

/**
 * Opens (or creates if needed) the SQLite database file and sets up the schema.
 *
 * Return: 0 on success, -1 on error
 */
int db_init(void);

/* This closes the database and flushes any pending writes */
void db_cleanup(void);

/**
 * This records that a film was watched by inserting a new row if we haven't
 * watched it before or incrementing the watch count and updating the timestamp
 * if we have.
 *
 * Return: 0 on success, -1 on error
 */
int db_insert(const char *path);

#endif
