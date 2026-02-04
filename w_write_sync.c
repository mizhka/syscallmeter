#include <sys/param.h>
#include <sys/mman.h>

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "syscallmeter.h"
#include "w_write_sync.h"

#define CHUNKSIZE 64 * 1024

/*
 * Joined - single hold of lock for write and sync
 * Dual - separate locks for write and sync
 * OnlyWrite - lock for write and no lock for sync
 * ExWr_ShSy - exclusive lock for write and shared lock for sync
 */
enum w_lockmode { JOINED, DUAL, ONLYWRITE, EXWR_SHSY };

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

typedef struct workers_sharedmem {
	sem_t mx_write;
	sem_t mx_sync;
	long position;
	int file_index;
} workers_sharedmem_t;

/* Global variables - constant over time */
typedef struct workers_test_params {
	enum w_lockmode w_mode;
	int sync_concurrency;
	int direct;
	int shift_position;
} workers_test_params_t;

struct workers_sharedmem *w_state = NULL;
struct workers_test_params w_params = { .sync_concurrency = 1,
	.w_mode = JOINED,
	.direct = 0,
	.shift_position = 0 };

int
w_write_sync_option(char *option)
{
	if (strcmp(option, "joined") == 0) {
		w_params.w_mode = JOINED;
	} else if (strcmp(option, "dual") == 0) {
		w_params.w_mode = DUAL;
	} else if (strcmp(option, "onlywrite") == 0) {
		w_params.w_mode = ONLYWRITE;
	} else if (strcmp(option, "sharesync8") == 0) {
		w_params.w_mode = EXWR_SHSY;
		w_params.sync_concurrency = 8;
	} else if (strcmp(option, "sharesync16") == 0) {
		w_params.w_mode = EXWR_SHSY;
		w_params.sync_concurrency = 16;
	} else if (strcmp(option, "direct") == 0) {
		w_params.direct = 1;
	} else if (strcmp(option, "doublelast") == 0) {
		w_params.shift_position = 8 * 1024;
	} else {
		printf("unexpected option: %s\n", option);
		return -1;
	}
	return (0);
}

int
w_write_sync_init(struct meter_settings *s, int dirfd)
{
	w_state = mmap(0, sizeof(struct workers_sharedmem),
	    PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1, 0);

	if (w_state == MAP_FAILED) {
		return (-1);
	}

	w_state->position = 0;
	w_state->file_index = 0;

	sem_init(&w_state->mx_write, 1, 1);
	sem_init(&w_state->mx_sync, 1, w_params.sync_concurrency);

	if (make_files(s, dirfd))
		return (-1);
	printf("Created files successfully\n");

	return (0);
}

long
w_write_sync_job(int workerid, struct meter_worker_state *s, int dirfd)
{
	char filename[128];
	int fd, err, curr_index, flags;
	ssize_t write_res;

	curr_index = w_state->file_index;

	char *data = alloc_rndbytes(s->settings->file_size);
	sprintf(filename, FNAME, curr_index);

	flags = O_CREAT | O_RDWR | (((w_params.direct != 0) ? O_DIRECT : 0));
	fd = openat(dirfd, filename, flags, 0644);

	for (long i = 0; i < s->settings->cycles; i++) {
		DO_WORK(20);

		DO_LOCK(&w_state->mx_write);

		if (curr_index < w_state->file_index) {
			close(fd);
			// TODO: err check
			curr_index = w_state->file_index;
			sprintf(filename, FNAME, curr_index);
			fd = openat(dirfd, filename, flags, 0644);
			// TODO: err check
		}

		pwrite(fd, &data[w_state->position],
		    MIN(CHUNKSIZE, s->settings->file_size - w_state->position),
		    w_state->position);
		// TODO: err check

		w_state->position += CHUNKSIZE - w_params.shift_position;
		if (w_state->position >= s->settings->file_size) {
			w_state->file_index++;
			w_state->position = 0;
		}

		switch (w_params.w_mode) {
		case DUAL:
		case EXWR_SHSY:
			DO_LOCK(&w_state->mx_sync);
			DO_UNLOCK(&w_state->mx_write);
			break;
		case ONLYWRITE:
			DO_UNLOCK(&w_state->mx_write);
			break;
		default:
			break;
		}

		err = fdatasync(fd);
		if (err != 0) {
			printf("fdatasync failed with error %s\n",
			    strerror(errno));
			exit(1);
		}

		s->my_stats->cycles++;

		switch (w_params.w_mode) {
		case JOINED:
			DO_UNLOCK(&w_state->mx_write);
			break;
		case DUAL:
		case EXWR_SHSY:
			DO_UNLOCK(&w_state->mx_sync);
			break;
		default:
			break;
		}
	}

	free(data);
	close(fd);

	return (s->my_stats->cycles);
}
