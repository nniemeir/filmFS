/**
 * operations.h
 *
 * Responsible for handling our FUSE callback functions.
 */

#ifndef OPERATIONS_H
#define OPERATIONS_H

/* This is the number of media player process names that we recognize */
#define NUM_OF_MEDIA_PLAYERS 2

/**
 * Return: Pointer to fuse_operations structure
 */
struct fuse_operations *get_operations(void);

#endif
