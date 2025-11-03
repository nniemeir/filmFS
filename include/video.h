#ifndef VIDEO_H
#define VIDEO_H

#include "common.h"
#include <dirent.h>

#define FILES_MAX 64
#define NUM_OF_VIDEO_EXTENSIONS 11

struct video_files {
  char **names;
  char **paths;
  unsigned int count;
};

struct video_files *get_files(void);

int library_init(void);

void files_cleanup(void);

#endif
