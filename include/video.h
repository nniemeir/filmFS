/**
 * video.h
 *
 * Responsible for constructing the file paths to each file in LIBARY_PATH and
 * extracting the basename for showing them in the mountpoint.
 */

#ifndef VIDEO_H
#define VIDEO_H

/**
 * The default maximum number of entries in the mountpoint directory, more
 * memory is allocated if this number is reached.
 */
#define FILES_MAX 64

/**
 * The current number of video extensions that we support.
 * 3gp, avi, flv, ogv, m4v, mov, mkv, mp4, mpg, mpeg, and webm
 */
#define NUM_OF_VIDEO_EXTENSIONS 11

/**
 * Contains information about the video files in LIBRARY_PATH.
 *
 * names - dynamically allocated array of video file basenames
 * paths - dynamically allocated array of video file paths
 * count - the number of video files in LIBRARY_PATH
 */
struct video_files {
  char **names;
  char **paths;
  unsigned int count;
};

/**
 * Return: a pointer to video_files struct instance
 */
struct video_files *get_files(void);

/**
 * This function opens the library directory, reads all the entries, filters for
 * video files, stores filenames and full paths in video_files struct, and
 * dynamically grows the arrays in the struct if there are more than 64 files.
 *
 * Return: 0 on success, -1 on error
 */
int library_init(void);

/**
 * We run this on program exit to free all dynamically allocated memory for file
 * lists. We free in the reverse of our allocation order, which is good
 * practice.
 */
void files_cleanup(void);

#endif
