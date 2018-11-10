// lock client interface.

#ifndef lock_client_cache_h

#define lock_client_cache_h

#include <string>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_client.h"
#include "lang/verify.h"
#include <pthread.h>
#include <map>

// Classes that inherit lock_release_user can override dorelease so that 
// that they will be called when lock_client releases a lock.
// You will not need to do anything with this class until Lab 6.
class lock_release_user {
 public:
  virtual void dorelease(lock_protocol::lockid_t) = 0;
  virtual ~lock_release_user() {};
};

class lock_client_cache : public lock_client {
 private:
  class lock_release_user *lu;
  int rlock_port;
  std::string hostname;
  std::string id;

  // ********** my design start here **********//

  enum lock_status_item { NONE=0, FREE, LOCKED, ACQUIRING, RELEASING };  

  struct lock_status_client {
    int status; //lock_status_item
    bool revoked; //whether lock has been revoked
    pthread_cond_t lock_status_cv; //similar to lab2 lock_server

    lock_status_client() {
      status = NONE;
      revoked = false;
      pthread_cond_init(&lock_status_cv, NULL);
    }
    ~lock_status_client() {
      pthread_cond_destroy(&lock_status_cv);
    }
  } 

  std::map<lock_protocol::lockid_t, lock_status_client*> lock_cache;
  pthread_mutex_t cache_mutex;

  void add_new_lock(lock_protocol::lockid_t);
  void set_lock_status(lock_status_client*, int);
  bool is_lock_status(lock_status_client*, int);
  lock_protocol::status acquire_actual(lock_protocol::lockid_t, lock_status_client*);
  lock_protocol::status release_actual(lock_protocol::lockid_t, lock_status_client*);

  // ********** my design finish here **********//

 public:
  static int last_port;
  lock_client_cache(std::string xdst, class lock_release_user *l = 0);
  virtual ~lock_client_cache() {};
  lock_protocol::status acquire(lock_protocol::lockid_t);
  lock_protocol::status release(lock_protocol::lockid_t);
  rlock_protocol::status revoke_handler(lock_protocol::lockid_t, int &);
  rlock_protocol::status retry_handler(lock_protocol::lockid_t, int &);
};


#endif
