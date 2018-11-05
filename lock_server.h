// this is the lock server
// the lock client has a similar interface

#ifndef lock_server_h
#define lock_server_h

#include <string>
#include "lock_protocol.h"
#include "lock_client.h"
#include "rpc.h"
#include <pthread.h>
#include <map>

class lock_server {

 /*
  * Function Definition
  *   int pthread_cond_init(pthread_cond_t *cv, const pthread_condattr_t *cattr);
  *   int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
  *   int pthread_cond_signal(pthread_cond_t *cond);
  *   int pthread_cond_destroy(...)
  *   int pthread_cond_timedwait(...)
  */

 private:
  struct lock_manager {
    bool granted;
    pthread_cond_t wait; //condition variable
    lock_manager() {
	granted = true;
  	pthread_cond_init(&wait, NULL);
    }
  };
 
  pthread_mutex_t map_mutex;
  std::map<lock_protocol::lockid_t, lock_manager> locks; //lid-lock

 protected:
  int nacquire; //number of acquire

 public:
  lock_server();
  ~lock_server() {};
  lock_protocol::status stat(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status acquire(int clt, lock_protocol::lockid_t lid, int &);
  lock_protocol::status release(int clt, lock_protocol::lockid_t lid, int &);
};

#endif 







