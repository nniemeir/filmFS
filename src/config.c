/**
 * config.c
 *
 * Configuration file parsing and management.
 *
 * OVERVIEW:
 * Responsible for reading and parsing the user's configuration at
 * ~/.config/filmfs/config.
 *
 * CONFIGURATION STRUCTURE:
 * We store the configuration context as a singleton that other files access
 * through get_config().
 */

#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"

static struct config_ctx config;

/**
 * get_config - Get pointer to the global configuration context
 *
 * Return: Pointer to configuration structure
 */
struct config_ctx *get_config(void) { return &config; }

/**
 * construct_config_path - Build full path to configuration file
 *
 * This constructs the path to the config file, which is
 * ~/.config/filmfs/config.
 *
 * Since we will need the home variable in db_init(), we store it in the
 * configuration context.
 *
 * Return: Configuration path on success, NULL on error
 */
char *construct_config_path(void) {
  /**
   * This is the home directory of the user running the program and is set
   * consistently. We need it to construct user-specific paths like ~/.config/.
   */
  config.home = getenv("HOME");
  if (!config.home) {
    fprintf(stderr, "Failed to get home directory.\n");
    return NULL;
  }

  /* PATH_MAX (4096 bytes) is the maximum path length on Linux */
  char *config_filename = malloc(PATH_MAX);
  if (!config_filename) {
    fprintf(stderr, "Memory allocation failed for configuration filename: %s",
            strerror(errno));
    return NULL;
  }

  /* We append the rest of the configuration file path */
  snprintf(config_filename, PATH_MAX, "%s/.config/filmfs/config", config.home);

  return config_filename;
}

/**
 * count_vars - Count the number of configuration variables in the file
 * @config_file_contents: String containing entire config file
 *
 * Counts the number of '=' characters to determine how many config variables
 * are present. We use this information to know how much memory to allocate for
 * our struct array.
 *
 * Return: Number of configuration variables found (minimum 1 to prevent calling
 * malloc with 0).
 */
unsigned int count_vars(char *config_file_contents) {
  unsigned int config_count = 0;

  /**
   * This might look a little confusing at first. We:
   * 1. Start at the beginning of the file
   * 2. Advance the pointer to the first occurence of '='
   * 3. Advance one more character to get us past '='
   * 4. Repeat for as many '=' as there are in the string
   */
  for (const char *temp = config_file_contents; (temp = strchr(temp, '='));
       temp += 1) {
    config_count++;
  }

  /* No '='s found, but we return 1 so that the caller won't try to call
   * malloc(0)*/
  if (config_count == 0) {
    return 1;
  }

  return config_count;
}

/**
 * cleanup_vars - Free all memory allocated for configuration variables
 *
 * Frees the dynamically allocated config variable names and values, then frees
 * the array itself. Called on shutdown or if parsing fails partway through.
 */
void cleanup_vars(void) {
  for (unsigned int i = 0; i < config.vars_count; i++) {
    if (config.vars[i].name) {
      free(config.vars[i].name);
    }
    if (config.vars[i].value) {
      free(config.vars[i].value);
    }
  }
  free(config.vars);
}

/** parse_configuration - Parse config file contents into struct array
 * @config_file_contents: String containing entire config file
 *
 * Parses KEY=VALUE lines from the config file and stores them in a struct.
 * Additionally, sets up library_path and debug based on the config.
 *
 * Return: 0 on success, -1 on error
 */
int parse_configuration(char *config_file_contents) {
  config.vars_count = count_vars(config_file_contents);

  /*
   * We cap the number of config variables to the number of supported config
   * variables. Currently we only support LIBRARY_PATH and DEBUG.
   */
  if (config.vars_count > NUM_OF_SUPPORTED_CONFIG) {
    fprintf(stderr, "Too many configuration values given.");
    return -1;
  }

  /* We only allocate room for as many config_pair structs as the file contains.
   */
  config.vars = malloc(config.vars_count * sizeof(struct config_pair));
  if (!config.vars) {
    fprintf(stderr, "Memory allocation failed for configuration values: %s",
            strerror(errno));
    return -1;
  }

  /*
   * We start tokenizing the file by lines. strtok() is pretty weird, this is
   * how it works:
   * - First call: pass the string to tokenize, the first delimiter is replaced
   * - Following calls: we pass NULL to continue replaces subsequent instances
   *   of delimiter
   * - Returns NULL when the delimiter is not found in the string
   */
  char *current_line = strtok(config_file_contents, "\n");
  unsigned int i = 0;

  while (current_line && i < config.vars_count) {
    // Find the next '='
    char *equal_sign = strchr(current_line, '=');

    if (!equal_sign) {
      /* Skip the current line if it doesn't have a '=' */
      current_line = strtok(NULL, "\n");
      continue;
    }

    /* We ensure that a value is actually assigned */
    if (*(equal_sign + 1) == '\0') {
      fprintf(stderr, "Empty config value.\n");
      return -1;
    }

    size_t name_len = equal_sign - current_line;
    size_t value_len = strlen(equal_sign + 1);

    /* Allocate memory for the config name */
    config.vars[i].name = malloc(name_len + 1);

    if (!config.vars[i].name) {
      fprintf(stderr,
              "Memory allocation failed for configuration value name %d: %s", i,
              strerror(errno));
      cleanup_vars();
      return -1;
    }

    /* Allocate memory for the config value */
    config.vars[i].value = malloc(value_len + 1);
    if (!config.vars[i].value) {
      fprintf(stderr, "Memory allocation failed for configuration value %d: %s",
              i, strerror(errno));
      cleanup_vars();
      return -1;
    }

    /* We copy the name into the space we allocated */
    strncpy(config.vars[i].name, current_line, name_len);
    config.vars[i].name[name_len] = '\0';

    /**
     * We copy the rest of the line after the equal_sign into the value space
     * that we allocated.
     */
    strcpy(config.vars[i].value, equal_sign + 1);

    i++;
    current_line = strtok(NULL, "\n");
  }

  /*
   * We loop through all parsed variables and handle the ones that we
   * recognize. We ignore any others rather than giving an error, which makes it
   * simple to add variables in the future.
   */
  for (unsigned int i = 0; i < config.vars_count; i++) {
    if (strcmp(config.vars[i].name, "LIBRARY_PATH") == 0) {
      /*
       * We don't bother duplicating the value since it is already in memory
       * that we allocated, and instead use a pointer.
       */
      config.library_path = config.vars[i].value;
      size_t library_path_len = strlen(config.library_path);

      /* We make sure that the library path ends in a '/' */
      if (config.library_path[library_path_len - 1] != '/') {
        if (library_path_len + 1 < PATH_MAX) {
          config.library_path[library_path_len] = '/';
          config.library_path[library_path_len + 1] = '\0';
        } else {
          fprintf(stderr, "PATH_MAX exceeded for library path.\n");
          cleanup_vars();
          return -1;
        }
      }
      continue;
    }
    if (strcmp(config.vars[i].name, "DEBUG") == 0) {
      if (strcmp(config.vars[i].value, "TRUE") == 0) {
        config.debug = 1;
      } else {
        config.debug = 0;
      }
    }
  }
  return 0;
}

/**
 * read_config_file - Read entire config file into memory
 * @config_file: Path to config file
 * @size: Size of config file in bytes
 *
 * Reads the config file into a malloc'd string using POSIX I/O rather than
 * stdio for consistency with our operations functions.
 *
 * Return: Malloc'd string containing config file contents, NULL on error
 */
char *read_config_file(char *config_file, const size_t size) {
  char *config_file_contents = malloc(size);

  /* We use the read only flag because we do not modify the file */
  int fd = open(config_file, O_RDONLY);
  if (fd == -1) {
    fprintf(stderr, "Failed to open %s: %s", config_file, strerror(errno));
    free(config_file_contents);
    return NULL;
  }

  if (read(fd, config_file_contents, size) == -1) {
    fprintf(stderr, "Failed to read from file for %s: %s", config_file,
            strerror(errno));
    free(config_file_contents);
    return NULL;
  }

  /**
   * Now that we have read the file into memory, we no longer need the file
   * descriptor.
   */
  if (close(fd) == -1) {
    fprintf(stderr, "Failed to close %s: %s", config_file, strerror(errno));
    free(config_file_contents);
    return NULL;
  }

  return config_file_contents;
}

/**
 * load_config - Load and parse the configuration file
 *
 * This is the entry point for configuration loading and calls the functions
 * defined above it.
 *
 * Return: 0 on success, -1 on error
 */
int load_config(void) {
  char *config_file = construct_config_path();
  if (!config_file) {
    return -1;
  }

  /* We verify that the file exists and that we have permission to read it. */
  struct stat buffer;
  if (stat(config_file, &buffer) == -1) {
    free(config_file);
    return -1;
  }

  /*
   * Since stat() fills the buffer with file metadata, we're able to pass the
   * file size here using the st_size field.
   */
  char *config_file_contents = read_config_file(config_file, buffer.st_size);
  if (!config_file_contents) {
    free(config_file);
    return -1;
  }

  /* Now that we have the contents of the file, we no longer need the path */
  free(config_file);

  if (parse_configuration(config_file_contents) == -1) {
    free(config_file_contents);
    return -1;
  }

  /*
   * We have copied all of the data we need from the file into our config
   * struct, so we can free this now.
   */
  free(config_file_contents);

  /*
   * We test that the library path actually exists and that the user has access
   * to it.
   */
  if (stat(config.library_path, &buffer) == -1) {
    fprintf(stderr, "Invalid LIBRARY_PATH: %s\n", strerror(errno));
    return -1;
  }

  /* 
   * We use the S_ISDIR macro to verify that LIBRARY_PATH is a directory rather
   * than a file, symlink, named pipe, or socket.
   */
  if (!S_ISDIR(buffer.st_mode)) {
    fprintf(stderr, "LIBRARY_PATH must refer to a directory.\n");
    return -1;
  }

  return 0;
}
