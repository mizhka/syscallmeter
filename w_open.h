#ifndef _W_OPEN_H_
#define _W_OPEN_H_

int w_open_init(struct meter_settings *,int);
long w_open_job(int, struct meter_worker_state *, int);

#endif /* !_W_OPEN_H_ */
