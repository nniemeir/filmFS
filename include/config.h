/**
 * config.h
 *
 * Responsible for handling configuration file parsing.
 */

#ifndef CONFIG_H
#define CONFIG_H

/* We currently only support DEBUG and LIBRARY_PATH as settings */
#define NUM_OF_SUPPORTED_CONFIG 2

/* This stores information about each setting in the config */
struct config_pair {
  char *name;
  char *value;
};

/**
 * We store a variety of configuration variables in this structure:
 * home - The path to the user's home directory (/home/user/)
 * library_path - Where the video files are actually located
 * debug - whether extensive error messages should be printed to stdout
 *         (currently not implemented)
 * vars_count - the number of settings specified in the configuration file
 * vars - the names and values of settings as given by the configuration file
 */
struct config_ctx {
  char *home;
  char *library_path;
  int debug;
  unsigned int vars_count;
  struct config_pair *vars;
};

/**
 * Accessor function to get pointer to config_ctx instance.
 *
 * Return: Pointer to config_ctx instance 
 */
struct config_ctx *get_config(void);

/**
 * load_config - Load and parse the configuration file
 *
 * This is the entry point for configuration loading.
 *
 * Return: 0 on success, -1 on error
 */
int load_config(void);

#endif
