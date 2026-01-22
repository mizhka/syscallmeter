#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "syscallmeter.h"
#include "w_open.h"

#define FNAME "file_%d"

int
w_open_init(int dirfd)
{
	if (make_files(dirfd))
		return (-1);
	printf("Created files successfully\n");
	return (0);
};

long
w_open_job(int workerid, int ncpu, int dirfd)
{
	char filename[128];
	int fd;
	long iter = 0;

	for (int i = 0; i < CYCLES; i++) {
		for (int k = 0; k < FILECOUNT; k++) {
			sprintf(filename, FNAME, k);
			fd = openat(dirfd, filename, O_RDWR);
			if (fd < 0) {
				printf("[%d] Can't create or open file %s",
				    workerid, filename);
				return -1;
			}
			close(fd);
			iter++;
		}
	}
	return (iter);
}
