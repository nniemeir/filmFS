#ifndef CONFIG_H
#define CONFIG_H

#include "common.h"

#define NUM_OF_SUPPORTED_CONFIG 2
#define MAX_CONFIG 16384

struct config_pair {
  char *name;
  char *value;
};

struct config_ctx {
  char *home;
  char *library_path;
  int debug;
  unsigned int vars_count;
  struct config_pair *vars;
};

struct config_ctx *get_config(void);
int load_config(void);

#endif
