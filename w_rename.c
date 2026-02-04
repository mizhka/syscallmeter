#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "syscallmeter.h"
#include "w_rename.h"

int
w_rename_init(struct meter_settings *s, int dirfd)
{
	if (make_files(s, dirfd))
		return (-1);
	printf("Created files successfully\n");
	return (0);
}

long
w_rename_job(int workerid, struct meter_worker_state *s, int dirfd)
{
	int renameRes;
	char filename[128];
	char newfilename[128];

	int file_id_start = (s->settings->file_count / s->settings->ncpu) *
	    workerid;
	int file_id_end = (s->settings->file_count / s->settings->ncpu) *
	    (workerid + 1);

	if (fchdir(dirfd)) {
		printf("[%d] Can't change dir: %s\n", workerid,
		    strerror(errno));
		return (-1);
	}

	for (long i = 0; i < s->settings->cycles; i++) {
		for (int file_id = file_id_start; file_id < file_id_end;
		    file_id++) {
			sprintf(filename, FNAME, file_id);
			sprintf(newfilename, FNAME,
			    file_id + s->settings->file_count);
			renameRes = rename(filename, newfilename);
			if (renameRes) {
				printf("[%d] Can't rename file %s to %s: %s",
				    workerid, filename, newfilename,
				    strerror(errno));
				return (-1);
			}
			s->my_stats->cycles++;
		}

		for (int file_id = file_id_start + s->settings->file_count;
		    file_id < file_id_end + s->settings->file_count;
		    file_id++) {
			sprintf(filename, FNAME, file_id);
			sprintf(newfilename, FNAME,
			    file_id - s->settings->file_count);
			renameRes = rename(filename, newfilename);
			if (renameRes) {
				printf("[%d] Can't rename file %s to %s: %s",
				    workerid, filename, newfilename,
				    strerror(errno));
				return (-1);
			}
			s->my_stats->cycles++;
		}
	}
	return (s->my_stats->cycles);
}
