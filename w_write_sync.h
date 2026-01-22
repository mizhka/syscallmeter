#ifndef _W_WRITE_SYNC_H_
#define _W_WRITE_SYNC_H_

int w_write_sync_joinedlock_init(int);
int w_write_sync_duallock_init(int);
int w_write_sync_onlywritelock_init(int);
long w_write_sync_job(int, int, int);

#endif /* !_W_WRITE_SYNC_H_ */
