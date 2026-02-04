#include <sys/param.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "syscallmeter.h"
#include "w_write_unlink.h"

int
w_write_unlink_init(struct meter_settings *s, int dirfd)
{
	if (make_files(s, dirfd))
		return (-1);
	printf("Created files successfully\n");
	return (0);
}

long
w_write_unlink_job(int workerid, struct meter_worker_state *s, int dirfd)
{
	char filename[128];
	int fd;
	ssize_t write_res;

	char *data = alloc_rndbytes(s->settings->file_size);
	sprintf(filename, FNAME, workerid);
	for (long i = 0; i < s->settings->cycles; i++) {
		fd = openat(dirfd, filename, O_CREAT | O_TRUNC | O_RDWR, 0644);
		if (fd < 0) {
			printf("[%d] Can't create or open file %s: %s\n",
			    workerid, filename, strerror(errno));
			close(fd);
			return (-1);
		}
		write_res = write(fd, data, s->settings->file_size);
		if (write_res != s->settings->file_size) {
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
		s->my_stats->cycles++;
	}
	free(data);

	return (s->my_stats->cycles);
}
