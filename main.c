/*
 * main.c
 *
 *  Created on: Aug 14, 2024
 *      Author: mizhka
 */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define CPULIMIT	128
#define CYCLES		1024
#define FILECOUNT	4 * 1024
#define FILESIZE	32 * 1024
#define FNAME		"file_%d"
#define TEMPDIR		"temp_syscallmeter"

typedef struct {
	sem_t fork_completed;
	sem_t starting;
} shmem_sem;

static char* alloc_rndbytes(size_t size);
static int init_directory(void);
static int make_files(int dirfd);
static int worker_job(int workerid, int dirfd);

int
main(int argc,char **argv)
{
	int dirfd, err;
	long ncpu;
	pid_t child;
	shmem_sem *semaphores;

	semaphores = mmap(0, sizeof(shmem_sem), PROT_READ | PROT_WRITE, MAP_ANON | MAP_SHARED, -1,0);

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
	if (dirfd < 0)
	{
		printf("Can\'t open directory\n");
		return -1;
	}
	printf("Created directory successfully\n");

	err = make_files(dirfd);
	if (err)
		return -1;
	printf("Created files successfully\n");

	ncpu = sysconf(_SC_NPROCESSORS_ONLN);
	printf("Found %ld cores\n",ncpu);
	ncpu = MIN(ncpu, CPULIMIT);

	for(long i = 0; i < ncpu; i++)
	{
		child = fork();
		if (child == 0) {
			cpu_set_t mask;
			double speed;
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
			worker_job(i, dirfd);
			clock_gettime(CLOCK_MONOTONIC, &ts_end);

			ts_end.tv_sec = ts_end.tv_sec - ts_start.tv_sec;
			ts_end.tv_nsec = ts_end.tv_nsec - ts_start.tv_nsec;
			if (ts_end.tv_nsec < 0) {
				ts_end.tv_nsec += 1000 * 1000 * 1000;
				ts_end.tv_sec -= 1;
			}

			speed = (double)((long long)ts_end.tv_sec * 1000 * 1000 * 1000 + ts_end.tv_nsec) / (double)((long)FILECOUNT * (long)CYCLES);

			printf("[%d] Worker is done with %ld in %lld.%.9ld sec (avg.time = %f ns)\n",
					child, (long)FILECOUNT * (long)CYCLES,
					(long long)ts_end.tv_sec, ts_end.tv_nsec, speed);
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
make_files(int dirfd)
{
	char* rndbytes;
	char filename[128];
	int fd;
	size_t written;

	rndbytes = alloc_rndbytes(FILESIZE);
	if (rndbytes == NULL)
	{
		printf("Can\'t allocate random bytes");
		return -1;
	}

	for (int k = 0; k < FILECOUNT; k++) {
		sprintf(filename, FNAME, k);
		fd = openat(dirfd, filename, O_CREAT | O_TRUNC | O_RDWR, 0644);
		if (fd < 0)
		{
			printf("Can't create or open file %s", filename);
			return -1;
		}
		written = write(fd, rndbytes, FILESIZE);
		if (written == -1)
		{
			printf("Can't write file %s: %s\n", filename, strerror(errno));
			return -1;
		}
		else if (written != FILESIZE)
		{
			printf("Can't fully write file %s: %zu\n", filename, written);
			return -1;
		}

		close(fd);
	}

	free(rndbytes);
	return 0;
}

static int
worker_job(int workerid,int dirfd)
{
	char filename[128];
	int fd;

	for(int i = 0; i < CYCLES; i++) {
		for (int k = 0; k < FILECOUNT; k++) {
			sprintf(filename, FNAME, k);
			fd = openat(dirfd, filename, O_RDWR);
			if (fd < 0)
			{
				printf("[%d] Can't create or open file %s", workerid, filename);
				return -1;
			}
			close(fd);
		}
	}
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
			printf("Can't create directory: %s\n",strerror(errno));
			return -1;
		}

		err = stat(TEMPDIR, &st);
		if (err) {
			printf("Error on stat(TEMPDIR): %s\n",strerror(errno));
			return -1;
		}

		if (!S_ISDIR(st.st_mode)) {
			printf("Error: Found non-directory with name " TEMPDIR "\n");
			return -1;
		}
		printf("Warning! Found old directory\n");
	}

	return 0;
}

static char*
alloc_rndbytes(size_t size)
{
	char* ret;

	ret = malloc(size);
	if (ret == NULL) {
		return ret;
	}

	for (int i = 0; i < size; i++)
	{
		ret[i]  = 'A' + (char) (random() % 0x1a);
	}

	return ret;
}
