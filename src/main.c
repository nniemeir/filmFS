/**
 * main.c
 *
 * Entry point for the program.
 *
 * OVERVIEW:
 * This file orchestrates the startup sequence and cleanup.
 *
 * fuse_main() blocks until the filesystem is unmounted. When it returns, we can
 * clean up resources.
 */

#include <stdlib.h>

#include "config.h"
#include "database.h"
#include "fuse.h"
#include "operations.h"
#include "video.h"

/**
 * main - Entry point
 * @argc: Argument count
 * @argv: Argument array
 *
 * Orchestrates the startup sequence and cleanup on unmount.
 *
 * Return: EXIT_SUCCESS or EXIT_FAILURE
 */
int main(int argc, char *argv[]) {
  /*
   * We load the configuration from ~/.config/filmfs/config and save the
   * settings within to our config struct, which we access from other files via
   * get_config().
   */
  if (load_config() == -1) {
    exit(EXIT_FAILURE);
  }

  /* We initialize the SQLite database and create the FILMS table if needed */
  if (db_init() == -1) {
    exit(EXIT_FAILURE);
  }

  /**
   * We store the names and paths of all video files in LIBRARY_PATH in memory
   * for the sake of efficiency.
   */
  if (library_init() == -1) {
    exit(EXIT_FAILURE);
  }

  /**
   * This is the entry point for the FUSE library. It parses argc and argv,
   * mounts the filesystem, starts the event loop to handle filesystem
   * operations, blocks until the filesystem is unmounted, then returns the exit
   * status.
   */
  int result = fuse_main(argc, argv, get_operations(), NULL);

  /* We free the cached names and path arrays*/
  files_cleanup();

  /* We close the SQLite database connection */
  db_cleanup();
  
  exit(result);
}
