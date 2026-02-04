#ifndef _W_CLOCK_GETTIME_H_
#define _W_CLOCK_GETTIME_H_

int w_clock_gettime_init(struct meter_settings *,int);
long w_clock_gettime_job(int, struct meter_worker_state *, int);
int w_clock_gettime_opt(char *);

#endif /* !_W_CLOCK_GETTIME_H_ */
