/**
 * operations.c
 *
 * FUSE filesystem operations implementation
 *
 * OVERVIEW:
 * FUSE is a framework that lets us implement filesystems in userspace programs
 * rather than kernel modules.
 *
 * FUSE PROCESS:
 * 1. The user program calls a system call on a file in our mountpoint
 * 2. Kernel intercepts the syscall
 * 3. FUSE kernel module forwards it to this program
 * 4. Our callback functions handle the request
 * 5. We return data/status to FUSE
 * 6. FUSE returns to the user program
 *
 * A FUSE filesystem can implement as many or as few of the filesystem syscalls
 * as they wish, we only concern ourselves with four of them:
 * - getattr: Get file attributes
 * - readdir: List directory contents
 * - open: Open a file
 * - read: Read file contents
 *
 * This is a read-only filesystem, so we don't need to implement modifying calls
 * like write().
 *
 * RETURN VALUES:
 * Notice that these functions return -ERRNO values on error: This is standard
 * behavior for raw syscalls, though their glibc wrappers (what gets called when
 * we call a syscall in a C program like stat() or write()) differ by returning
 * -1 on error and setting errno.
 */

#include <errno.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "config.h"
#include "database.h"
#include "fuse.h"
#include "operations.h"
#include "video.h"

/**
 * get_file_status - Get metadata for a file in our virtual filesystem
 *
 * Maps a FUSE path to the actual file using the full paths we constructed
 * previously and retrieves its metadata with stat().
 *
 * Return: 0 on success, -ENOENT if file not found
 */
static int get_file_status(const char *path, struct stat *file_stat) {
  struct video_files *files = get_files();
  /* Linear search through all video files, this could be changed to a more
   * efficient method in a future commit. */
  for (unsigned int i = 0; i < files->count; i++) {
    /* Skip the leading slash in path */
    if (strcmp(path + 1, files->names[i]) == 0) {
      /**
       * We get the metadata for the real file, which is important because FUSE
       * needs to report accurate sizes to function properly.
       */
      int result = stat(files->paths[i], file_stat);
      if (result == -1) {
        fprintf(stderr, "Failed to get file status for %s: %s", path,
                strerror(errno));
        return result;
      }
      return 0;
    }
  }

  /* Return NO ENTry, the standard POSIX error code for a nonexistent file. */
  return -ENOENT;
}

/**
 * fs_getattr - FUSE getattr callback
 * @path: Path to file/directory
 * @st: Output buffer for file attributes
 *
 * This gets called when a program stats a file. We get the metadata for the
 * real file and populate the stat struct with it.
 *
 * Return: 0 on success, -errno on failure
 */
static int fs_getattr(const char *path, struct stat *st) {
  static const int dir_permissions = 0755;  // RWX for owner, RX otherwise
  static const int file_permissions = 0644; // RW for owner, R otherwise

  /*
   * Files will appear to be owned by the user ID of the process running
   * the filesystem.
   */
  st->st_uid = getuid(); // The uid of the file owner

  /*
   * Files will appear to have the owner group as the user's primary group*/
  st->st_gid = getgid();

  /* Set access and modification times to the current time */
  time_t current_time = time(NULL);
  if (current_time == (time_t)-1) {
    fprintf(stderr, "Failed to get time for atime: %s", strerror(errno));
    return -errno;
  }

  st->st_atime = current_time; // Last access time
  st->st_mtime = current_time; // Last modification time

  if (strcmp(path, "/") == 0) {
    /* Mark as a directory with the proper permissions. */
    st->st_mode = S_IFDIR | dir_permissions;
    /**
     * Directories have link count >= 2, one link from parent directory, one
     * from "." (self reference), and one link for each subdirectory's "..".
     * Since we don't support subdirectories, we set it to 2.
     */
    st->st_nlink = 2;
    return 0;
  }

  /* We set the permissions for regular files */
  st->st_mode = S_IFREG | file_permissions;
  /* Regular files typically have link count of 1 (no hard links) */
  st->st_nlink = 1;

  /* Get the real file's metadata so we can find its size */
  struct stat file_stat;
  int result = get_file_status(path, &file_stat);
  if (result != 0) {
    return result;
  }

  /**
   * Copy the real file size. This is important because programs need to know
   * how big files are to do things like allocating buffers and showing progress
   * bars.
   */
  st->st_size = file_stat.st_size;

  return 0;
}

/**
 * fs_readdir - FUSE readdir callback
 * @path: Directory path to list
 * @buffer: FUSE buffer to fill with entries
 * @filler: FUSE function to add entries to buffer
 * @offset: Offset for pagination (irrelevant to our usecase)
 * @fi: File info (irrelevant to our usecase)
 *
 * This is called when a program lists directory contents. We only support
 * listing the root directory. We return all the video files from the library
 * path.
 *
 * FUSE provides a callback function 'filler' that adds one entry to the
 * directory listing per call.
 *
 * Return: 0 on success
 */
static int fs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi) {
  /**
   * FUSE requires these parameters in the function signature, but we don't use
   * them. Casting to void prevents GCC from whining about unnused parameters.
   */
  (void)offset;
  (void)(fi);

  struct video_files *files = get_files();

  /*
   * We add standard UNIX directories to allow for proper directory navigation.
   * If we ommitted them, tools like cd and pwd would be borked.
   */
  filler(buffer, ".", NULL, 0);  // Current Directory
  filler(buffer, "..", NULL, 0); // Parent Directory

  /**
   * We only list files if we are looking at the root directories, as we do not
   * support subdirectories.
   */
  if (strcmp(path, "/") == 0) {
    for (unsigned int i = 0; i < files->count; i++) {
      filler(buffer, files->names[i], NULL, 0);
    }
  }

  return 0;
}

/**
 * get_proc_name - Get name of process making FUSE request
 *
 * Reads /proc/<pid>/comm to determine if the program accessing our filesystem
 * is a media player.
 *
 * PROC FILESYSTEM:
 * Linux exposes process information through /proc/. Each process has a
 * directory /proc/<pid/ that contains various information files. We only care
 * about comm, which gives us the command name.
 *
 * Return: Allocated string with process name, or NULL on error
 */
char *get_proc_name(void) {
  /*
   * The kernel truncates any process names longer than 16 characters including
   * null terminator.
   */
  static const int proc_comm_len = 16;

  char *proc_name = malloc(proc_comm_len);
  if (!proc_name) {
    fprintf(stderr, "Memory allocation failed for full_path: %s",
            strerror(errno));
    return NULL;
  }

  /* We build the path to the comm file */
  char *proc_path = malloc(PATH_MAX);
  if (!proc_path) {
    fprintf(stderr, "Memory allocation failed for full_path: %s",
            strerror(errno));
    free(proc_name);
    return NULL;
  }
  /* fuse_get_context() returns information about the current FUSE request
   * including:
   * - pid: Process ID of the program making the request
   * - uid: User ID
   * - gid: Group ID
   * - private_data: Custom data that we could store per-mount
   */
  snprintf(proc_path, PATH_MAX, "/proc/%d/comm", fuse_get_context()->pid);

  /* We open the comm file for reading. */
  int fd = open(proc_path, O_RDONLY);
  if (fd == -1) {
    fprintf(stderr, "Failed to open %s: %s", proc_path, strerror(errno));
    free(proc_name);
    free(proc_path);
    return NULL;
  }

  /* We read the process name into proc_name */
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

  /* We don't need the file anymore, so we close the file descriptor */
  if (close(fd) == -1) {
    fprintf(stderr, "Failed to close %s: %s", proc_path, strerror(errno));
    free(proc_name);
    free(proc_path);
    return NULL;
  }

  free(proc_path);

  /* Remove the trailing newline from the process name, replacing it with '\0'
   */
  proc_name[strcspn(proc_name, "\n")] = 0;

  return proc_name;
}

/**
 * logging_handle - Log film viewing if request is from media player
 * @path: FUSE path being accessed
 *
 * We detect when media players read files and log those accesses as watches.
 *
 * DETECTION STRATEGY:
 * 1. Get the name of the process making the request
 * 2. Check if it's a known media player
 * 3. If it is, and it's a new process (not a continued read), we log it
 *
 * SUPPORTED MEDIA PLAYERS:
 * - "demux": VLC's demuxer thread
 * - "vlc:disk$0": VLC disk reading thread
 *
 * These are VLC-specific thread names. To support other players, we'd add their
 * process names to the array.
 *
 * We track the last PID we logged to avoid duplicate entries, as players make
 * many read() calls for a single viewing.
 *
 * Return: 0 on success, -ERRNO on failure
 */
int logging_handle(const char *path) {
  static const char *media_player_comm[NUM_OF_MEDIA_PLAYERS] = {"demux",
                                                                "vlc:disk$0"};
  /* We initialize this to -1 since it is an impossible PID */
  static pid_t last_pid = -1;

  /* Get the name of the process making the request */
  char *proc_name = get_proc_name();
  if (!proc_name) {
    fprintf(stderr, "FAILED TO GET PROCESS NAME\n");
    return -EIO;
  }

  /* Check if this process is a known media player */
  int caller_is_media_player = 0;
  for (unsigned int i = 0; i < NUM_OF_MEDIA_PLAYERS; i++) {
    if (strcmp(proc_name, media_player_comm[i]) == 0) {
      caller_is_media_player = 1;
      break;
    }
  }

  free(proc_name);

  /* Get the PID of the calling process */
  pid_t current_pid = fuse_get_context()->pid;

  /**
   * Only log to database if the caller is a media player and this is a new
   * process. We do this because media players might make thousands of read()
   * calls during one viewing.
   */
  if (caller_is_media_player == 1 && current_pid != last_pid) {
    last_pid = current_pid;
    if (db_insert(path) == -1) {
      return -EFAULT;
    }
  }

  return 0;
}

/**
 * fs_read - FUSE read callback
 * @path: Path to file being read
 * @buffer: Buffer to fill with file data
 * @size: Number of bytes requested
 * @offset: Position in file to read from
 * @fi: File info structure (contains file descriptor if we opened it)
 *
 * This is called when a program reads from a file in our filesystem. We call
 * logging_handle() first to potentially log the access before actually reading
 * the file.
 *
 * We use pread() which reads from a specific offset without changing the file
 * position. This is important because multiple threads might read from the same
 * file simultaneously.
 *
 * Return: Number of bytes read on success, -ERRNO on failure
 */
static int fs_read(const char *path, char *buffer, size_t size, off_t offset,
                   struct fuse_file_info *fi) {
  struct video_files *files = get_files();

  /* Attempt to log access */
  int log_res = logging_handle(path);
  if (log_res != 0) {
    fprintf(stderr, "Failed to log read.\n");
    return log_res;
  }

  int fd;
  ssize_t result;

  /* Find the file in our list and read it */
  for (unsigned int i = 0; i < files->count; i++) {
    if (strcmp(path + 1, files->names[i]) != 0) {
      continue;
    }

    /* Build the full path to read file in libary */
    char *full_path = malloc(PATH_MAX);
    if (!full_path) {
      fprintf(stderr, "Memory allocation failed for full_path: %s",
              strerror(errno));
      return -errno;
    }

    snprintf(full_path, PATH_MAX, "%s%s", get_config()->library_path,
             files->names[i]);

    /**
     * Use file descriptor from fi if available, otherwise open the file.
     *
     * If fs_open() was called first, fi->fh contains the file descriptor.
     * Otherwise, fi is NULL and we need to open it ourselves.
     */
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

    /**
     * We use a loop for reading the data because pread() can return fewer bytes
     * than requested. We keep reading until we get everything or hit an
     * error/EOF.
     */
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

    /**
     * If we opened the file, we have to close it. If fs_open opened it,
     * fs_open()'s corresponding close will happen later.
     */
    if (fi == NULL) {
      if (close(fd) == -1) {
        fprintf(stderr, "Failed to close %s: %s", full_path, strerror(errno));
        free(full_path);
        return -errno;
      }
    }

    free(full_path);

    return bytes_read;
  }
  return -ENOENT;
}

/**
 * fs_open - FUSE open callback
 * @path: Path to file being opened
 * @fi: File info structure to store file descriptor
 *
 * This is called when a program opens a file. We open the real file and store
 * the file descriptor in fi->fh so that subsequent read() calls can use it.
 *
 * fi->fh is where we store the file descriptor. FUSE passes the fi structure to
 * read() and release() calls, which allows us to avoid repeatedly opening and
 * closing the file.
 *
 * Return: 0 on success, -ERRNO on failure
 */
static int fs_open(const char *path, struct fuse_file_info *fi) {
  struct video_files *files = get_files();

  char *full_path = malloc(PATH_MAX);
  if (!full_path) {
    fprintf(stderr, "Memory allocation failed for full_path: %s",
            strerror(errno));
    return -errno;
  }

  /* Find the file and open it */
  for (unsigned int i = 0; i < files->count; i++) {
    if (strcmp(path + 1, files->names[i]) == 0) {
      snprintf(full_path, PATH_MAX, "%s%s", get_config()->library_path,
               files->names[i]);
      int fd = open(full_path, O_RDONLY);
      if (fd == -1) {
        fprintf(stderr, "Failed to open %s: %s", full_path, strerror(errno));
        free(full_path);
        return -errno;
      }
      free(full_path);

      /**
       * We store the file descriptor in FUSE file info structure so that
       * subsequent read calls can read from the file without reopening it.
       */
      fi->fh = fd;
      return 0;
    }
  }
  return -ENOENT;
}

/**
 * This struct tells FUSE which functions to call for each filesystem operation.
 * FUSE supports many operations, but we only need to implement the operations
 * that are needed for a read_only filesystem.
 */
static struct fuse_operations operations = {.getattr = fs_getattr,
                                            .readdir = fs_readdir,
                                            .read = fs_read,
                                            .open = fs_open};

/**
 * get_operations - Get pointer to FUSE operations structure
 *
 * Return: Pointer to fuse_operations structure
 */
struct fuse_operations *get_operations(void) { return &operations; }
