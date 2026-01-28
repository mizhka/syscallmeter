#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "syscallmeter.h"
#include "w_open.h"

#define FNAME "file_%d"

int
w_open_init(struct meter_settings *s, int dirfd)
{
	if (make_files(s, dirfd))
		return (-1);
	printf("Created files successfully\n");
	return (0);
};

long
w_open_job(int workerid, struct meter_worker_state *s, int dirfd)
{
	char filename[128];
	int fd;

	for (int i = 0; i < s->settings->cycles; i++) {
		for (int k = 0; k < s->settings->file_count; k++) {
			sprintf(filename, FNAME, k);
			fd = openat(dirfd, filename, O_RDWR);
			if (fd < 0) {
				printf("[%d] Can't create or open file %s",
				    workerid, filename);
				return -1;
			}
			close(fd);
			s->my_stats->cycles++;
		}
	}
	return (s->my_stats->cycles);
}
