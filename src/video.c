#include "video.h"
#include "common.h"
#include <ctype.h>
#include <linux/limits.h>

static const char *video_extensions[NUM_OF_VIDEO_EXTENSIONS] = {"mp4", "mkv",
                                                                "avi"};

static struct video_files files;

struct video_files *get_files(void) { return &files; }

void files_cleanup(void) {
  for (unsigned int i = 0; i < files.count; i++) {
    if (files.names[i]) {
      free(files.names[i]);
    }
    if (files.paths[i]) {
      free(files.paths[i]);
    }
  }
  free(files.names);
  free(files.paths);
}

static int has_video_extension(const char *filename) {
  char *file_extension = strrchr(filename, '.');
  if (!file_extension) {
    return 0;
  }

  file_extension++;

  for (int i = 0; file_extension[i]; i++) {
    file_extension[i] = tolower(file_extension[i]);
  }

  for (unsigned int i = 0; i < NUM_OF_VIDEO_EXTENSIONS; i++) {
    if (strcmp(file_extension, video_extensions[i]) == 0) {
      return 1;
    }
  }
  return 0;
}

int library_init(void) {
  unsigned int buffer_size = FILES_MAX;

  files.names = malloc(buffer_size * sizeof(char *));
  if (!files.names) {
    fprintf(stderr, "Memory allocation failed for files.names: %s",
            strerror(errno));
    return 1;
  }

  files.paths = malloc(buffer_size * sizeof(char *));
  if (!files.paths) {
    fprintf(stderr, "Memory allocation failed for files.paths: %s",
            strerror(errno));
    return 1;
  }

  files.count = 0;

  struct dirent *dp;
  DIR *dir = opendir(LIBRARY_PATH);
  if (!dir) {
    fprintf(stderr, "Failed to open directory %s: %s", LIBRARY_PATH,
            strerror(errno));
    free(files.names);
    free(files.paths);
    return 1;
  }

  while ((dp = readdir(dir))) {
    if (dp->d_type == DT_REG) {
      if (has_video_extension(dp->d_name) == 0) {
        continue;
      }

      files.names[files.count] = strdup(dp->d_name);
      if (files.names[files.count] == NULL) {
        fprintf(stderr, "Failed to duplicate d_name to files.names[%d]: %s",
                files.count, strerror(errno));
        return 1;
      }

      files.paths[files.count] = malloc(PATH_MAX);
      if (!files.paths[files.count]) {
        fprintf(stderr, "Memory allocation failed for files.names[%d]: %s",
                files.count, strerror(errno));
        free(files.names[files.count]);
        for (int i = (files.count - 1); i >= 0; i--) {
          free(files.names[i]);
          free(files.paths[i]);
        }
        free(files.names);
        free(files.paths);
        return 1;
      }

      snprintf(files.paths[files.count], PATH_MAX, "%s%s", LIBRARY_PATH,
               dp->d_name);

      files.count++;

      if (files.count == (unsigned int)buffer_size) {
        buffer_size += FILES_MAX;

        char **names_tmp = realloc(files.names, buffer_size * sizeof(char *));
        if (names_tmp == NULL) {
          fprintf(stderr, "Memory reallocation failed for files.names: %s",
                  strerror(errno));
          files_cleanup();
          return 1;
        } else {
          files.names = names_tmp;
        }

        char **paths_tmp = realloc(files.paths, buffer_size * sizeof(char *));
        if (paths_tmp == NULL) {
          fprintf(stderr, "Memory reallocation failed for files.names: %s",
                  strerror(errno));
          files_cleanup();
          return 1;
        } else {
          files.paths = paths_tmp;
        }
      }
    }
  }

  if (closedir(dir) == -1) {
    fprintf(stderr, "Failed to close directory %s: %s", LIBRARY_PATH,
            strerror(errno));
    files_cleanup();
    return 1;
  }

  return 0;
}
