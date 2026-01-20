#include <sys/param.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "syscallmeter.h"
#include "w_write_sync.h"

int
w_write_sync_init(int dirfd)
{
	if (make_files(dirfd))
		return (-1);
	printf("Created files successfully\n");
	return (0);
}

int
w_write_sync_job(int workerid, int ncpu, int dirfd)
{
	char filename[128];
	int fd;
	ssize_t write_res;
	char *data = alloc_rndbytes(FILESIZE);
	sprintf(filename, FNAME, workerid);
	for (int i = 0; i < CYCLES; i++) {
		fd = openat(dirfd, filename, O_CREAT | O_TRUNC | O_RDWR, 0644);
		if (fd < 0) {
			printf("[%d] Can't create or open file %s: %s\n",
			    workerid, filename, strerror(errno));
			close(fd);
			return (-1);
		}
		write_res = write(fd, data, FILESIZE);
		if (write_res != FILESIZE) {
			printf("[%d] Can't write file %s: %s\n", workerid,
			    filename, strerror(errno));
			close(fd);
			return (-1);
		}
		close(fd);
		if (unlinkat(dirfd, filename, 0) < 0) {
			printf("[%d] Can't unlick file %s: %s\n", workerid,
			    filename, strerror(errno));
		}
	}
	free(data);

	return (0);
}
