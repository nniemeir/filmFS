#include "common.h"
#include "operations.h"
#include "video.h"

int main(int argc, char *argv[]) {
  if (library_init() == 1) {
    exit(EXIT_FAILURE);
  }
  int result = fuse_main(argc, argv, get_operations(), NULL);
  files_cleanup();
  exit(result);
}
