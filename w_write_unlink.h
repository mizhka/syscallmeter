#ifndef _W_WRITE_UNLINK_H_
#define _W_WRITE_UNLINK_H_

int w_write_unlink_init(struct meter_settings *, int);
long w_write_unlink_job(int, struct meter_worker_state *, int);

#endif /* !_W_WRITE_UNLINK_H_ */
