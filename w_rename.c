#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "syscallmeter.h"
#include "w_rename.h"

int
w_rename_init(int dirfd)
{
	if (make_files(dirfd))
		return (-1);
	printf("Created files successfully\n");
	return (0);
}

int
w_rename_job(int workerid, int ncpu, int dirfd)
{
	int renameRes;
	char filename[128];
	char newfilename[128];

	int file_id_start = (FILECOUNT / ncpu) * workerid;
	int file_id_end = (FILECOUNT / ncpu) * (workerid + 1);

	if (fchdir(dirfd)) {
		printf("[%d] Can't change dir: %s\n", workerid,
		    strerror(errno));
		return (-1);
	}

	for (int i = 0; i < CYCLES; i++) {
		for (int file_id = file_id_start; file_id < file_id_end;
		    file_id++) {
			sprintf(filename, FNAME, file_id);
			sprintf(newfilename, FNAME, file_id + FILECOUNT);
			renameRes = rename(filename, newfilename);
			if (renameRes) {
				printf("[%d] Can't rename file %s to %s: %s",
				    workerid, filename, newfilename,
				    strerror(errno));
				return (-1);
			}
		}

		for (int file_id = file_id_start + FILECOUNT;
		    file_id < file_id_end + FILECOUNT; file_id++) {
			sprintf(filename, FNAME, file_id);
			sprintf(newfilename, FNAME, file_id - FILECOUNT);
			renameRes = rename(filename, newfilename);
			if (renameRes) {
				printf("[%d] Can't rename file %s to %s: %s",
				    workerid, filename, newfilename,
				    strerror(errno));
				return (-1);
			}
		}
	}
	return (0);
}
