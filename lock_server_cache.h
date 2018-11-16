#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>

#include <map>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"
#include <pthread.h>

 /*
  * Function Definition
  *   int pthread_cond_init(pthread_cond_t *cv, const pthread_condattr_t *cattr);
  *   int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
  *   int pthread_cond_signal(pthread_cond_t *cond);
  *   int pthread_cond_destroy(...)
  *   int pthread_cond_timedwait(...)
  */

class lock_server_cache {
 private:
  
  // ********** my design start here **********//
  struct list {
    std::string value;
    list* next;
    list() {value = ""; next = NULL;}
    list(std::string v) {value = v; next = NULL;}

    void insert(std::string v){
        list* tail = new list();
        tail->value = v;
        list* cur = this;
        while (cur->next != NULL)
          cur = cur->next;
        cur->next = tail;
    }
  };

  struct lock_status_server { //paras of each lock
    bool granted; //locked
    std::string owner; //client ip:port
    list* wait_list;

    lock_status_server() {
      granted = false;
      owner = "";
      wait_list = NULL;
    }
  };

  std::map<lock_protocol::lockid_t, lock_status_server*> lock_map;  //lid-lock
  pthread_mutex_t map_mutex;

  void add_new_lock(lock_protocol::lockid_t, std::string);
  rlock_protocol::status rcall(std::string, int, lock_protocol::lockid_t);
  
  // ********** my design finish here **********//

  int nacquire;
 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
