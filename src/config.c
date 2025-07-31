#include "config.h"

static struct config_ctx config;

struct config_ctx *get_config(void) { return &config; }

char *construct_config_path(void) {
  config.home = getenv("HOME");
  if (!config.home) {
    fprintf(stderr, "Failed to get home directory.\n");
    return NULL;
  }

  char *config_filename = malloc(PATH_MAX);
  if (!config_filename) {
    fprintf(stderr, "Memory allocation failed for configuration filename: %s",
            strerror(errno));
    return NULL;
  }

  snprintf(config_filename, PATH_MAX, "%s/.config/filmfs/config", config.home);

  return config_filename;
}

unsigned int count_vars(char *config_file_contents) {
  unsigned int config_count = 0;
  for (const char *temp = config_file_contents; (temp = strchr(temp, '='));
       temp += 1) {
    config_count++;
  }

  if (config_count == 0) {
    return 1;
  }

  return config_count;
}

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

int parse_configuration(char *config_file_contents) {
  config.vars_count = count_vars(config_file_contents);
  if (config.vars_count > NUM_OF_SUPPORTED_CONFIG) {
    fprintf(stderr, "Too many configuration values given.");
    return 1;
  }

  config.vars = malloc(config.vars_count * sizeof(struct config_pair));
  if (!config.vars) {
    fprintf(stderr, "Memory allocation failed for configuration values: %s",
            strerror(errno));
    return 1;
  }

  char *current_line = strtok(config_file_contents, "\n");
  unsigned int i = 0;

  while (current_line && i < config.vars_count) {
    char *equal_sign = strchr(current_line, '=');

    if (!equal_sign) {
      current_line = strtok(NULL, "\n");
      continue;
    }

    if (*(equal_sign + 1) == '\0') {
      fprintf(stderr, "Empty config value.\n");
      return 1;
    }

    size_t name_len = equal_sign - current_line;
    size_t value_len = strlen(equal_sign + 1);

    config.vars[i].name = malloc(name_len + 1);

    if (!config.vars[i].name) {
      fprintf(stderr,
              "Memory allocation failed for configuration value name %d: %s", i,
              strerror(errno));
      cleanup_vars();
      return 1;
    }

    config.vars[i].value = malloc(value_len + 1);
    if (!config.vars[i].value) {
      fprintf(stderr, "Memory allocation failed for configuration value %d: %s",
              i, strerror(errno));
      cleanup_vars();
      return 1;
    }

    strncpy(config.vars[i].name, current_line, name_len);
    config.vars[i].name[name_len] = '\0';

    strcpy(config.vars[i].value, equal_sign + 1);

    i++;
    current_line = strtok(NULL, "\n");
  }

  for (unsigned int i = 0; i < config.vars_count; i++) {
    if (strcmp(config.vars[i].name, "LIBRARY_PATH") == 0) {
      config.library_path = config.vars[i].value;
      continue;
    }
    if (strcmp(config.vars[i].name, "DEBUG") == 0) {
      if (strcmp(config.vars[i].value, "TRUE") == 0) {
        config.debug = 1;
      }
    }
  }
  return 0;
}

char *read_config_file(char *config_file, const size_t size) {
  char *config_file_contents = malloc(size);
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

  if (close(fd) == -1) {
    fprintf(stderr, "Failed to close %s: %s", config_file, strerror(errno));
    free(config_file_contents);
    return NULL;
  }

  return config_file_contents;
}

int load_config(void) {
  char *config_file = construct_config_path();
  if (!config_file) {
    return 1;
  }

  struct stat buffer;
  if (stat(config_file, &buffer) == -1) {
    free(config_file);
    return 1;
  }

  char *config_file_contents = read_config_file(config_file, buffer.st_size);
  if (!config_file_contents) {
    free(config_file);
    return 1;
  }

  free(config_file);

  if (parse_configuration(config_file_contents) == 1) {
    free(config_file_contents);
    return 1;
  }

  free(config_file_contents);

  return 0;
}
