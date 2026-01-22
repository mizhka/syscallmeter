/*
 * main.c
 *
 *  Created on: Aug 14, 2024
 *      Author: mizhka
 */

#include "w_write_sync.h"
#define _GNU_SOURCE

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "syscallmeter.h"
#include "w_open.h"
#include "w_rename.h"
#include "w_write_sync.h"
#include "w_write_unlink.h"

#define CPULIMIT_DEF  128
#define CYCLES_DEF    1024
#define FILECOUNT_DEF 4 * 1024
#define FILESIZE_DEF  32 * 1024
#define TEMPDIR_DEF   "temp_syscallmeter"
#define MODE_DEF      "open"

typedef struct {
	sem_t fork_completed;
	sem_t starting;
} shmem_sem;

int cpu_limit = CPULIMIT_DEF;
int cycles = CYCLES_DEF;
int file_count = FILECOUNT_DEF;
int file_size = FILESIZE_DEF;
char *temp_dir = TEMPDIR_DEF;
char *mode = MODE_DEF;
long ncpu;

static int init_directory(void);
static int parse_opts(int argc, char **argv);

int
main(int argc, char **argv)
{
	int dirfd, err;

	pid_t child;
	shmem_sem *semaphores;
	worker_func func;

	if (parse_opts(argc, argv) != 0)
		return -1;

	semaphores = mmap(0, sizeof(shmem_sem), PROT_READ | PROT_WRITE,
	    MAP_ANON | MAP_SHARED, -1, 0);

	if (semaphores == MAP_FAILED) {
		printf("Can't mmap area: %s\n", strerror(errno));
		return -1;
	}

	sem_init(&semaphores->fork_completed, 1, 0);
	sem_init(&semaphores->starting, 1, 0);

	err = init_directory();
	if (err)
		return -1;

	dirfd = open(TEMPDIR, 0);
	if (dirfd < 0) {
		printf("Can\'t open directory\n");
		return -1;
	}
	printf("Created directory successfully\n");

	if (strcmp(MODE, "open") == 0) {
		func.init = &w_open_init;
		func.job = &w_open_job;
	} else if (strcmp(MODE, "rename") == 0) {
		func.init = &w_rename_init;
		func.job = &w_rename_job;
	} else if (strcmp(MODE, "write_unlink") == 0) {
		func.init = &w_write_unlink_init;
		func.job = &w_write_unlink_job;
	} else if (strcmp(MODE, "write_sync_joinedlock") == 0) {
		func.init = &w_write_sync_joinedlock_init;
		func.job = &w_write_sync_job;
	} else if (strcmp(MODE, "write_sync_duallock") == 0) {
		func.init = &w_write_sync_duallock_init;
		func.job = &w_write_sync_job;
	} else if (strcmp(MODE, "write_sync_onlywritelock") == 0) {
		func.init = &w_write_sync_onlywritelock_init;
		func.job = &w_write_sync_job;
	} else {
		printf("Unknown worker job (-m): %s,"
		       " use -h to see valid job names\n",
		    MODE);
		return -1;
	}

	err = func.init(dirfd);

	for (long i = 0; i < ncpu; i++) {
		child = fork();
		if (child == 0) {
			cpu_set_t mask;
			double speed;
			long iter;

			struct timespec ts_start, ts_end;

			child = getpid();

			CPU_ZERO(&mask);
			CPU_SET(i, &mask);
			err = sched_setaffinity(0, sizeof(cpu_set_t), &mask);
			if (err == -1) {
				printf("[%d] Can\'t set affinity\n", child);
				return -1;
			}
			printf("[%d] I\'m on CPU: %d\n", child, sched_getcpu());
			sem_post(&semaphores->fork_completed);
			sem_wait(&semaphores->starting);
			clock_gettime(CLOCK_MONOTONIC, &ts_start);

			iter = func.job(i, ncpu, dirfd);

			clock_gettime(CLOCK_MONOTONIC, &ts_end);

			ts_end.tv_sec = ts_end.tv_sec - ts_start.tv_sec;
			ts_end.tv_nsec = ts_end.tv_nsec - ts_start.tv_nsec;
			if (ts_end.tv_nsec < 0) {
				ts_end.tv_nsec += 1000 * 1000 * 1000;
				ts_end.tv_sec -= 1;
			}

			speed = (double)((long long)ts_end.tv_sec * 1000 *
					1000 * 1000 +
				    ts_end.tv_nsec) /
			    (double)(iter);

			printf(
			    "[%d / %ld] Worker is done with %ld in %lld.%.9ld sec (avg.time = %f ns)\n",
			    child, i, iter, (long long)ts_end.tv_sec,
			    ts_end.tv_nsec, speed);
			return 0;
		}
	}

	for (int i = 0; i < ncpu; i++) {
		sem_wait(&semaphores->fork_completed);
	}

	printf("Starting...\n");
	for (int i = 0; i < ncpu; i++) {
		sem_post(&semaphores->starting);
	}

	do {
		child = wait(&child);
	} while (child > 0);

	printf("Done\n");
	return 0;
}

static int
parse_opts(int argc, char **argv)
{
	int opt;
	while ((opt = getopt(argc, argv, "j:c:f:s:d:m:h")) != -1) {
		switch (opt) {
		case 'j':
			cpu_limit = strtol(optarg, NULL, 10);
			if (errno == EINVAL || errno == ERANGE ||
			    cpu_limit <= 0) {
				printf(
				    "invalid arg %s for option -j expected integer grater than 0\n",
				    optarg);
				return -1;
			}
			break;

		case 'c':
			cycles = strtol(optarg, NULL, 10);
			if (errno == EINVAL || errno == ERANGE || cycles <= 0) {
				printf(
				    "invalid arg %s for option -c expected integer grater than 0\n",
				    optarg);
				return -1;
			}
			break;
		case 'f':
			file_count = strtol(optarg, NULL, 10);
			if (errno == EINVAL || errno == ERANGE ||
			    file_count <= 0) {
				printf(
				    "invalid arg %s for option -f expected integer grater than 0\n",
				    optarg);
				return -1;
			}
			break;
		case 's':
			file_size = strtol(optarg, NULL, 10);
			if (errno == EINVAL || errno == ERANGE ||
			    file_size <= 0) {
				printf(
				    "invalid arg %s for option -s expected integer grater than 0\n",
				    optarg);
				return -1;
			}
			break;
		case 'd':
			temp_dir = optarg;
			break;
		case 'm':
			mode = optarg;
			break;
		case 'h':
			printf(
			    "Usage:\n"
			    " -c number of cycles, default %d\n"
			    " -d directory path, default %s\n"
			    " -f number of files to create, default %d\n"
			    " -h no arg, use to dispay this message\n"
			    " -j number of max number of cpu, default %d\n"
			    " -m defines worker job, valid jobs: open, rename, write_unlink. Default %s\n"
			    " -s number of bytes in each file, default %d\n",
			    CYCLES_DEF, TEMPDIR_DEF, FILECOUNT_DEF,
			    CPULIMIT_DEF, MODE_DEF, FILESIZE_DEF);
			return -1;
		default:
			printf("unexpected option %c", opt);
			break;
		}
	}

	ncpu = sysconf(_SC_NPROCESSORS_ONLN);
	printf("Found %ld cores\n", ncpu);
	ncpu = MIN(ncpu, CPULIMIT);

	printf("Settings:\n");
	printf("\tCYCLES = %d\n", cycles);
	printf("\tWORKERS = %ld\n", ncpu);
	printf("\tFILECOUNT = %d\n", file_count);
	printf("\tFILESIZE = %d\n", file_size);

	return 0;
}

int
make_files(int dirfd)
{
	char *rndbytes;
	char filename[128];
	int fd;
	size_t written;

	rndbytes = alloc_rndbytes(FILESIZE);
	if (rndbytes == NULL) {
		printf("Can\'t allocate random bytes");
		return -1;
	}

	for (int k = 0; k < FILECOUNT; k++) {
		sprintf(filename, FNAME, k);
		fd = openat(dirfd, filename, O_CREAT | O_TRUNC | O_RDWR, 0644);
		if (fd < 0) {
			printf("Can't create or open file %s", filename);
			return -1;
		}
		written = write(fd, rndbytes, FILESIZE);
		if (written == -1) {
			printf("Can't write file %s: %s\n", filename,
			    strerror(errno));
			return -1;
		} else if (written != FILESIZE) {
			printf("Can't fully write file %s: %zu\n", filename,
			    written);
			return -1;
		}

		close(fd);
	}

	free(rndbytes);
	return 0;
}

static int
init_directory(void)
{
	int err;
	struct stat st;

	err = mkdir(TEMPDIR, 0775);
	if (err) {
		if (errno != EEXIST) {
			printf("Can't create directory: %s\n", strerror(errno));
			return -1;
		}

		err = stat(TEMPDIR, &st);
		if (err) {
			printf("Error on stat(TEMPDIR): %s\n", strerror(errno));
			return -1;
		}

		if (!S_ISDIR(st.st_mode)) {
			printf("Error: Found non-directory with name %s\n",
			    temp_dir);
			return -1;
		}
		printf("Warning! Found old directory\n");
	}

	return 0;
}

char *
alloc_rndbytes(size_t size)
{
	char *ret;
	unsigned int g_seed;
	g_seed = random();

	ret = malloc(size);
	if (ret == NULL) {
		return ret;
	}

	for (int i = 0; i < size; i++) {
		g_seed = (214013 * g_seed + 2531011);
		ret[i] = 'A' + (char)(((g_seed >> 16) & 0x7FFF) % 0x1a);
	}

	return ret;
}
