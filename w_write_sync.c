#include <sys/param.h>
#include <sys/mman.h>

// #include <errno.h>
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
// #include <string.h>
#include <unistd.h>

#include "syscallmeter.h"
#include "w_write_sync.h"

#define CHUNKSIZE 64 * 1024

enum w_lockmode { JOINED, DUAL, ONLYWRITE };

#define DO_LOCK(_p)                                                                \
	do {                                                                       \
		err = sem_wait((_p));                                              \
		if (err != 0) {                                                    \
			switch (err) {                                             \
			case EINVAL:                                               \
				printf(                                            \
				    "The argument points to invalid semaphore\n"); \
				exit(-1);                                          \
			case EINTR:                                                \
				exit(-1);                                          \
			}                                                          \
		}                                                                  \
	} while (1 == 0);

#define DO_UNLOCK(_p)                                                              \
	do {                                                                       \
		err = sem_post((_p));                                              \
		if (err != 0) {                                                    \
			switch (err) {                                             \
			case EINVAL:                                               \
				printf(                                            \
				    "The argument points to invalid semaphore\n"); \
				exit(-1);                                          \
			case EOVERFLOW:                                            \
				printf("Unexpected overflow of semaphore\n");      \
				exit(-1);                                          \
			}                                                          \
		}                                                                  \
	} while (1 == 0);

#define DO_WORK(_p)                                             \
	do {                                                    \
		unsigned long long start, end;                  \
		start = __rdtsc();                              \
		unsigned long long wait_cycles = 3000 * ((_p)); \
		do {                                            \
			end = __rdtsc();                        \
		} while ((end - start) < wait_cycles);          \
	} while (1 == 0);

typedef struct workers_sem {
	sem_t mx_write;
	sem_t mx_sync;
	long position;
	int file_index;
	enum w_lockmode w_mode;
} workers_sem_t;

struct workers_sem *w_state;

#include <x86intrin.h>

static int
w_write_sync_init_common(int dirfd, enum w_lockmode w_mode)
{
	w_state = mmap(0, sizeof(struct workers_sem), PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0);

	sem_init(&w_state->mx_write, 1, 1);
	sem_init(&w_state->mx_sync, 1, 1);

	w_state->position = 0;
	w_state->file_index = 0;
	w_state->w_mode = w_mode;

	if (make_files(dirfd))
		return (-1);
	printf("Created files successfully\n");
	return (0);
}

int
w_write_sync_joinedlock_init(int dirfd)
{
	return (w_write_sync_init_common(dirfd, JOINED));
}

int
w_write_sync_duallock_init(int dirfd)
{
	return (w_write_sync_init_common(dirfd, DUAL));
}

int
w_write_sync_onlywritelock_init(int dirfd)
{
	return (w_write_sync_init_common(dirfd, ONLYWRITE));
}

long
w_write_sync_job(int workerid, int ncpu, int dirfd)
{
	char filename[128];
	int fd, err, curr_index;
	ssize_t write_res;
	long iter;

	iter = 0;
	curr_index = w_state->file_index;

	char *data = alloc_rndbytes(FILESIZE);
	sprintf(filename, FNAME, curr_index);
	fd = openat(dirfd, filename, O_CREAT | O_RDWR, 0644);

	for (int i = 0; i < CYCLES; i++) {
		DO_WORK(20);
		DO_LOCK(&w_state->mx_write);

		if (curr_index < w_state->file_index) {
			close(fd);
			// TODO: err check
			curr_index = w_state->file_index;
			sprintf(filename, FNAME, curr_index);
			fd = openat(dirfd, filename, O_CREAT | O_RDWR, 0644);
			// TODO: err check
		}

		pwrite(fd, &data[w_state->position], CHUNKSIZE,
		    w_state->position);
		// TODO: err check
		w_state->position += CHUNKSIZE;
		if (w_state->position >= file_size) {
			w_state->file_index++;
			w_state->position %= file_size;
		}

		switch (w_state->w_mode) {
		case DUAL:
			DO_LOCK(&w_state->mx_sync);
			DO_UNLOCK(&w_state->mx_write);
			break;
		case ONLYWRITE:
			DO_UNLOCK(&w_state->mx_write);
			break;
		default:
			break;
		}

		fdatasync(fd);
		iter++;

		switch (w_state->w_mode) {
		case JOINED:
			DO_UNLOCK(&w_state->mx_write);
			break;
		case DUAL:
			DO_UNLOCK(&w_state->mx_sync);
			break;
		default:
			break;
		}
	}

	free(data);
	close(fd);

	return (iter);
}
