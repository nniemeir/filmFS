#include "operations.h"
#include "common.h"
#include "config.h"
#include "database.h"
#include "video.h"

static int get_file_status(const char *path, struct stat *file_stat) {
  for (unsigned int i = 0; i < get_files()->count; i++) {
    if (strcmp(path + 1, get_files()->names[i]) == 0) {
      int result = stat(get_files()->paths[i], file_stat);
      if (result == -1) {
        fprintf(stderr, "Failed to get file status for %s: %s", path,
                strerror(errno));
        return result;
      }
      return 0;
    }
  }
  return -ENOENT;
}

static int fs_getattr(const char *path, struct stat *st) {
  static const int dir_permissions = 0755;  // RWX for owner, RX otherwise
  static const int file_permissions = 0644; // RW for owner, R otherwise

  st->st_uid = getuid(); // The uid of the file owner

  st->st_gid = getgid(); // The owner group of the files/directories

  time_t current_time = time(NULL);
  if (current_time == (time_t)-1) {
    fprintf(stderr, "Failed to get time for atime: %s", strerror(errno));
    return -errno;
  }

  st->st_atime = current_time; // Last access time
  st->st_mtime = current_time; // Last modification time

  if (strcmp(path, "/") == 0) {
    st->st_mode = S_IFDIR | dir_permissions;
    st->st_nlink = 2; // Number of hardlinks
    return 0;
  }

  st->st_mode = S_IFREG | file_permissions;
  st->st_nlink = 1; // Number of hardlinks
  struct stat file_stat;
  int result = get_file_status(path, &file_stat);
  if (result != 0) {
    return result;
  }
  st->st_size = file_stat.st_size; // Filesize

  return 0;
}

// filler is a function sent by fuse and can be used to fill buffer with
// available files in path
static int fs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi) {
  (void)offset;
  (void)(fi);
  filler(buffer, ".", NULL, 0);  // Current Directory
  filler(buffer, "..", NULL, 0); // Parent Directory

  if (strcmp(path, "/") == 0) {
    for (unsigned int i = 0; i < get_files()->count; i++) {
      filler(buffer, get_files()->names[i], NULL, 0);
    }
  }

  return 0;
}

char *get_proc_name(void) {
  static const int proc_comm_len =
      16; // Maximum length allowed for this file, see proc_pid_comm manpage

  char *proc_name = malloc(proc_comm_len);
  if (!proc_name) {
    fprintf(stderr, "Memory allocation failed for full_path: %s",
            strerror(errno));
    return NULL;
  }

  char *proc_path = malloc(PATH_MAX);
  if (!proc_path) {
    fprintf(stderr, "Memory allocation failed for full_path: %s",
            strerror(errno));
    free(proc_name);
    return NULL;
  }
  snprintf(proc_path, PATH_MAX, "/proc/%d/comm", fuse_get_context()->pid);

  int fd = open(proc_path, O_RDONLY);
  if (fd == -1) {
    fprintf(stderr, "Failed to open %s: %s", proc_path, strerror(errno));
    free(proc_name);
    free(proc_path);
    return NULL;
  }

  if (read(fd, proc_name, proc_comm_len) == -1) {
    fprintf(stderr, "Failed to read from file for %s: %s", proc_path,
            strerror(errno));
    if (close(fd) == -1) {
      fprintf(stderr, "Failed to close %s: %s", proc_path, strerror(errno));
    }
    free(proc_name);
    free(proc_path);
    return NULL;
  }

  if (close(fd) == -1) {
    fprintf(stderr, "Failed to close %s: %s", proc_path, strerror(errno));
    free(proc_name);
    free(proc_path);
    return NULL;
  }

  free(proc_path);

  proc_name[strcspn(proc_name, "\n")] = 0;

  return proc_name;
}

int logging_handle(const char *path) {
  static const char *media_player_comm[NUM_OF_MEDIA_PLAYERS] = {"demux",
                                                                "vlc:disk$0"};
  static pid_t last_pid = -1;
  char *proc_name = get_proc_name();
  if (!proc_name) {
    fprintf(stderr, "FAILED TO GET PROCESS NAME\n");
    return -EIO;
  }

  int caller_is_media_player = 0;
  for (unsigned int i = 0; i < NUM_OF_MEDIA_PLAYERS; i++) {
    if (strcmp(proc_name, media_player_comm[i]) == 0) {
      caller_is_media_player = 1;
      break;
    }
  }

  free(proc_name);

  pid_t current_pid = fuse_get_context()->pid;

  if (caller_is_media_player == 1 && current_pid != last_pid) {
    last_pid = current_pid;
    if (db_insert(path) == 1) {
      return -EFAULT;
    }
  }

  return 0;
}

static int fs_read(const char *path, char *buffer, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
  int log_res = logging_handle(path);
  if (log_res != 0) {
    fprintf(stderr, "Failed to log read.\n");
    return log_res;
  }
  int fd;
  ssize_t result;
  for (unsigned int i = 0; i < get_files()->count; i++) {
    if (strcmp(path + 1, get_files()->names[i]) != 0) {
      continue;
    }

    char *full_path = malloc(PATH_MAX);
    if (!full_path) {
      fprintf(stderr, "Memory allocation failed for full_path: %s",
              strerror(errno));
      return -errno;
    }

    snprintf(full_path, PATH_MAX, "%s%s", get_config()->library_path,
             get_files()->names[i]);

    if (fi == NULL) {
      fd = open(full_path, O_RDONLY);
    } else {
      fd = fi->fh;
    }

    if (fd == -1) {
      fprintf(stderr, "Failed to open %s: %s", full_path, strerror(errno));
      free(full_path);
      return -errno;
    }

    size_t bytes_read = 0;
    while (bytes_read < size) {
      result = pread(fd, buffer + bytes_read, size - bytes_read,
                     offset + bytes_read);
      if (result <= 0) {
        break;
      }
      bytes_read += result;
    }

    if (result == -1) {
      fprintf(stderr, "Failed to read from file for %s: %s", full_path,
              strerror(errno));
      free(full_path);
      return -errno;
    }

    if (fi == NULL) {
      if (close(fd) == -1) {
        fprintf(stderr, "Failed to close %s: %s", full_path, strerror(errno));
        free(full_path);
        return -errno;
      }
    }

    free(full_path);

    return result;
  }
  return -ENOENT;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
  char *full_path = malloc(PATH_MAX);
  if (!full_path) {
    fprintf(stderr, "Memory allocation failed for full_path: %s",
            strerror(errno));
    return -errno;
  }

  for (unsigned int i = 0; i < get_files()->count; i++) {
    if (strcmp(path + 1, get_files()->names[i]) == 0) {
      snprintf(full_path, PATH_MAX, "%s%s", get_config()->library_path,
               get_files()->names[i]);
      int fd = open(full_path, O_RDONLY);
      if (fd == -1) {
        fprintf(stderr, "Failed to open %s: %s", full_path, strerror(errno));
        free(full_path);
        return -errno;
      }
      free(full_path);
      fi->fh = fd;
      return 0;
    }
  }
  return -ENOENT;
}

static struct fuse_operations operations = {.getattr = fs_getattr,
                                            .readdir = fs_readdir,
                                            .read = fs_read,
                                            .open = fs_open};

struct fuse_operations *get_operations(void) { return &operations; }
