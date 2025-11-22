/**
 * video.c
 *
 * Video file discovery and management.
 *
 *
 * OVERVIEW:
 * Handles scanning the LIBRARY_PATH directory to find all video files and
 * storing them in our video_files struct for quick access.
 */
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "video.h"

/*
 * This is our cache of video files found in LIBRARY_PATH, we populate it once
 * at startup and then read it many times in our FUSE operations.
 */
static struct video_files files;

/*
 * get_files - Get pointer to video_files structure
 *
 * Used by our FUSE operations in operations.c to look up files.
 *
 * Return: Pointer to video_files structure
 */
struct video_files *get_files(void) { return &files; }

/**
 * files_cleanup - free all dynamically allocated memory for file lists.
 *
 * We run this on program exit. We free in the reverse of our allocation order,
 * which is good practice.
 */
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

/**
 * has_video_extension - Check if filename has a recognized video extension
 * @filename: Filename to check
 *
 * Determines if a file is a video by checking its extension against a list of
 * known video formats.
 *
 * We normalize the provided extension by using tolower on each character during
 * comparison.
 *
 * Return: true if file has video extension, false otherwise
 */
static bool has_video_extension(const char *filename) {
  /**
   * We use static so that we allocate the array once rather than on every call,
   * and use const because the array does not need to be modified once defined.
   */
  static const char *video_extensions[NUM_OF_VIDEO_EXTENSIONS] = {
      "3gp", "avi", "flv", "ogv",  "m4v", "mov",
      "mkv", "mp4", "mpg", "mpeg", "webm"};

  /* We get the position of the last '.' in the filename. */
  char *file_extension = strrchr(filename, '.');
  if (!file_extension) {
    return false;
  }

  /* We skip past the dot */
  file_extension++;

  /* We convert each character in file_extension to lowercase */
  for (int i = 0; file_extension[i]; i++) {
    file_extension[i] = tolower(file_extension[i]);
  }

  /**
   * Then we try to find a match in the array. This is a small array so linear
   * search is fine.
   */
  for (unsigned int i = 0; i < NUM_OF_VIDEO_EXTENSIONS; i++) {
    if (strcmp(file_extension, video_extensions[i]) == 0) {
      return true;
    }
  }
  return false;
}

/**
 * library_init - Scan LIBRARY_PATH and build list of video files
 *
 * This function opens the library directory, reads all the entries, filters for
 * video files, stores filenames and full paths in video_files struct, and
 * dynamically grows the arrays in the struct if there are more than 64 files.
 *
 * Return: 0 on success, -1 on error
 */
int library_init(void) {
  const char *library_path = get_config()->library_path;

  /* Initial size for the arrays in video_files, but we realloc if needed */
  unsigned int buffer_size = FILES_MAX;

  files.names = malloc(buffer_size * sizeof(char *));
  if (!files.names) {
    fprintf(stderr, "Memory allocation failed for files.names: %s",
            strerror(errno));
    return -1;
  }

  files.paths = malloc(buffer_size * sizeof(char *));
  if (!files.paths) {
    fprintf(stderr, "Memory allocation failed for files.paths: %s",
            strerror(errno));
    return -1;
  }

  files.count = 0;

  /*
   * Open the library directory for reading. This gives us a DIR* that we use
   * with readdir to iterate through entries. DIR* must be closed with
   * closedir() when we are done with them.
   */
  struct dirent *dp;
  DIR *dir = opendir(library_path);
  if (!dir) {
    fprintf(stderr, "Failed to open directory %s: %s", library_path,
            strerror(errno));
    free(files.names);
    free(files.paths);
    return -1;
  }

  /* We iterate through all of the directory entries. readdir() returns a
   * pointer to the next entry, or NULL when done. */
  while ((dp = readdir(dir))) {
    /* We only care about regular files, not directories or special files */
    if (dp->d_type == DT_REG) {
      if (!has_video_extension(dp->d_name)) {
        continue;
      }

      /*
       * We duplicate the filename into our names array because dp->d_name
       * points to readdir() state that will be overwritten on the next call.
       */
      files.names[files.count] = strdup(dp->d_name);
      if (files.names[files.count] == NULL) {
        fprintf(stderr, "Failed to duplicate d_name to files.names[%d]: %s",
                files.count, strerror(errno));
        return -1;
      }

      /* We allocate space for the full file path */
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
        return -1;
      }

      /* Build the full path */
      snprintf(files.paths[files.count], PATH_MAX, "%s%s", library_path,
               dp->d_name);

      files.count++;

      /*
       * We check if we have filled the arrays and expand them by FILES_MAX (64)
       * if so.
       */
      if (files.count == (unsigned int)buffer_size) {
        buffer_size += FILES_MAX;

        /*
         * We use a temporary variable here so that we don't lose the original
         * pointer if realloc fails.
         */
        char **names_tmp = realloc(files.names, buffer_size * sizeof(char *));
        if (names_tmp == NULL) {
          fprintf(stderr, "Memory reallocation failed for files.names: %s",
                  strerror(errno));
          files_cleanup();
          return -1;
        } else {
          files.names = names_tmp;
        }

        char **paths_tmp = realloc(files.paths, buffer_size * sizeof(char *));
        if (paths_tmp == NULL) {
          fprintf(stderr, "Memory reallocation failed for files.names: %s",
                  strerror(errno));
          files_cleanup();
          return -1;
        } else {
          files.paths = paths_tmp;
        }
      }
    }
  }

  /* 
   * Directory streams are like file descriptors in that they are a finite
   * resource, so it is important to close them when we are finished using them.
   */
  if (closedir(dir) == -1) {
    fprintf(stderr, "Failed to close directory %s: %s", library_path,
            strerror(errno));
    files_cleanup();
    return -1;
  }

  return 0;
}
