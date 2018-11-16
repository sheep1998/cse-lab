// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"
#include <map>

/* 
 * tprintf(args) 
 * string id: IP::port
 * lock_protocol status: OK, RETRY, RPCERR, NOENT, IOERR
 */

lock_server_cache::lock_server_cache()
{
}

int 
lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, int &)
{
  lock_protocol::status ret = lock_protocol::OK;

  pthread_mutex_lock(&map_mutex);

  //not find lock entry
  if (lock_map.find(lid) == lock_map.end()) {
    add_new_lock(lid, id);
  }

  lock_status_server* ls = lock_map[lid];
  
  //lock is occupied
  if (ls->granted) {
    if (ls->wait_list == NULL){
      ls->wait_list = new list(id);
    } else {
      ls->wait_list->insert(id); //add to wait list
    }

    ret = lock_protocol::RETRY;
    pthread_mutex_unlock(&map_mutex);

    while (rcall(ls->owner, rlock_protocol::revoke, lid) != rlock_protocol::OK){
      tprintf("send revoke %llu to %s fail.\n", lid, ls->owner.c_str());
    }
  }
  //lock is free
  else {
    ls->granted = true;
    ls->owner = id;
    pthread_mutex_unlock(&map_mutex);
  }

  return ret;
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;

  pthread_mutex_lock(&map_mutex);
  bool toRPC = false;

  //not find lock entry
  if (lock_map.find(lid) == lock_map.end()) {
    tprintf("release %llu from %s: no entry\n", lid, id.c_str());
    pthread_mutex_unlock(&map_mutex);
    return lock_protocol::NOENT;
  }

  lock_status_server* ls = lock_map[lid];
  
  //lock is occupied
  if (ls->granted) {
    if (ls->owner == id){
        ls->granted = false;
        ls->owner = "";

        //wake up the wait client
        if (ls->wait_list == NULL){
          //code here
        } else {
          ls->granted = true;
          ls->owner = ls->wait_list->value;
          ls->wait_list = ls->wait_list->next;
          toRPC = true;
        }
    } else {
      tprintf("release %llu from %s: do not own this lock!\n", lid, id.c_str());
      ret = lock_protocol::NOENT;
    }
  }
  //lock is free
  else {
    tprintf("release %llu from %s: do not own this lock!\n", lid, id.c_str());
    ret = lock_protocol::NOENT;
  }

  pthread_mutex_unlock(&map_mutex);

  //wake up client
  if (toRPC){
    while (rcall(ls->owner, rlock_protocol::retry, lid) != rlock_protocol::OK){
      tprintf("send retry %llu to %s fail.\n", lid, id.c_str());
    }

    if (ls->wait_list != NULL){
      while (rcall(ls->owner, rlock_protocol::revoke, lid) != rlock_protocol::OK){
        tprintf("send revoke %llu to %s fail.\n", lid, id.c_str());
      }
    } else {
      //code here
    }
  }

  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

void 
lock_server_cache::add_new_lock(lock_protocol::lockid_t lid, std::string id)
{
  lock_status_server* new_lock = new lock_status_server();
  new_lock->owner = id;
  lock_map.insert(std::pair<lock_protocol::lockid_t, lock_status_server*>(lid, new_lock));
};

//rpc call: retry and revoke
rlock_protocol::status 
lock_server_cache::rcall(std::string id, int protocol, lock_protocol::lockid_t lid)
{
  int r;
  lock_protocol::status ret = lock_protocol::OK;

  handle h(id); //handle.h, class handle
  rpcc* cl = h.safebind();

  if (cl){
    ret = cl->call(protocol, lid, r);
  } else {
    ret = lock_protocol::RPCERR;
  }

  return ret;
};

