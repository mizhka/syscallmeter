#ifndef _SYSCALLMETER_H_
#define _SYSCALLMETER_H_

#include <sys/param.h>

extern int cpu_limit;
extern int cycles;
extern int file_count;
extern int file_size;
extern char *temp_dir;
extern char *mode;

#define FNAME	"file_%d"

#define CPULIMIT  cpu_limit
#define CYCLES	  cycles
#define FILECOUNT file_count
#define FILESIZE  file_size
#define TEMPDIR	  temp_dir
#define MODE	  mode

typedef int (*worker_init_t)(int);
/*
 * args: workerid, ncpu, dirfd
 * returns: positive - amount of iterations
 *          negative - error
 */
typedef long (*worker_job_t)(int, int, int);

typedef struct {
	worker_init_t init;
	worker_job_t job;
} worker_func;

int make_files(int dirfd);
char *alloc_rndbytes(size_t size);

#endif /* !_SYSCALLMETER_H_ */
