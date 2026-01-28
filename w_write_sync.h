#ifndef _W_WRITE_SYNC_H_
#define _W_WRITE_SYNC_H_


int w_write_sync_option(char*);

int w_write_sync_init(struct meter_settings *, int);
long w_write_sync_job(int, struct meter_worker_state *, int);

#endif /* !_W_WRITE_SYNC_H_ */
