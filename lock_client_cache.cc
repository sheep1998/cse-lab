// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"
#include <pthread.h>

/*
 * int acquire(lock_protocol::lockid_t, std::string id, int &);
 * int release(lock_protocol::lockid_t, std::string id, int &);
 * enum lock_status_item { NONE=0, FREE, LOCKED, ACQUIRING, RELEASING };
 */

int lock_client_cache::last_port = 0;

lock_client_cache::lock_client_cache(std::string xdst, 
class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  last_port = rlock_port;

  printf("lock client cache init\n");

  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);
}

/*
lock_client_cache::~lock_client_cache()
{
  pthread_mutex_destroy(&cache_mutex);
} 
*/

//cache acquire
lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  lock_protocol::status ret = lock_protocol::OK;

  pthread_mutex_lock(&cache_mutex);
  bool toRPC = false;

  //not find lock entry
  if (lock_cache.find(lid) == lock_cache.end()){
    add_new_lock(lid);
  }

  lock_status_client* ls = lock_cache[lid];

  //status: NONE
  if (ls->status == NONE){
    ls->status = ACQUIRING;
    toRPC = true;
  }
  //status: FREE
  else if (ls->status == FREE){
    ls->status = LOCKED;
  }
  //status: LOCKED, ACQUIRING, RELEASING
  else {
    do {
      pthread_cond_wait(&ls->lock_status_cv, &cache_mutex);
    } while ((ls->status != NONE) && (ls->status != FREE));

    if (ls->status == NONE){
      ls->status = ACQUIRING;
      toRPC = true;
    } else {
       ls->status = LOCKED;
    }
  }

  pthread_mutex_unlock(&cache_mutex);

  if (toRPC) {
    ret = acquire_actual(lid, ls);
  }

  return ret;
}

//cache release
lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  lock_protocol::status ret = lock_protocol::OK;

  pthread_mutex_lock(&cache_mutex);
  bool toRPC = false;

  //not find lock entry
  if (lock_cache.find(lid) == lock_cache.end()) {
    tprintf("release %llu: no entry\n", lid);
    pthread_mutex_unlock(&cache_mutex);
    return lock_protocol::NOENT;
  }
  
  lock_status_client* ls = lock_cache[lid];

  //status should be lock
  if (ls->status != LOCKED){
    tprintf("release %llu: status not locked!\n", lid);
    ret = lock_protocol::NOENT;
  }
  //locked but not revoked
  else if (ls->revoked == false){
    ls->status = FREE;
    pthread_cond_signal(&ls->lock_status_cv);
  }
  //locked and revoked
  else {
    ls->status = RELEASING;
    ls->revoked = false;
    toRPC = true;
  }

  pthread_mutex_unlock(&cache_mutex);

  if (toRPC){
    ret = release_actual(lid, ls);
  }

  return ret;
}

//revoke handler
rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, int &)
{
  lock_protocol::status ret = rlock_protocol::OK;
  
  pthread_mutex_lock(&cache_mutex);
  bool toRPC = false;

  //not find lock entry
  if (lock_cache.find(lid) == lock_cache.end()) {
    tprintf("release %llu: no entry\n", lid);
    pthread_mutex_unlock(&cache_mutex);
    return ret;
  }
  
  lock_status_client* ls = lock_cache[lid];

  //status: NONE
  if (ls->status == NONE){
    //do nothing
  }
  //status: FREE
  else if (ls->status == FREE){
    ls->status = RELEASING;
    ls->revoked = false;
    toRPC = true;
  }
  //status: LOCKED, ACQUIRING, RELEASING
  else {
    ls->revoked = true;
  }

  pthread_mutex_unlock(&cache_mutex);

  if (toRPC){
    ret = release_actual(lid, ls);
  }

  return ret;
}

//retry handler
rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
int &)
{
  lock_protocol::status ret = rlock_protocol::OK;
 
  pthread_mutex_lock(&cache_mutex);

  //not find lock entry
  if (lock_cache.find(lid) == lock_cache.end()) {
    tprintf("retry %llu: no entry!\n", lid);
  } 
  //retry
  else {
    lock_status_client* ls = lock_cache[lid];
    if (ls->status == ACQUIRING) {
      ls->status = LOCKED;
    } else {
      //do nothing 
    }
  }
  
  pthread_mutex_unlock(&cache_mutex);

  return ret;
}

//add new lock to map
void 
lock_client_cache::add_new_lock(lock_protocol::lockid_t lid)
{
  lock_status_client* new_lock = new lock_status_client();
  lock_cache.insert(std::pair<lock_protocol::lockid_t, lock_status_client*>(lid, new_lock));
}

//set lock status
void 
lock_client_cache::set_lock_status(lock_status_client* ls, int status)
{
  pthread_mutex_lock(&cache_mutex);
  ls->status = status;  
  pthread_mutex_unlock(&cache_mutex);
}

//compare lock status
bool 
lock_client_cache::is_lock_status(lock_status_client* ls, int status)
{
  pthread_mutex_lock(&cache_mutex);
  bool ret = (ls->status == status);  
  pthread_mutex_unlock(&cache_mutex);
  return ret;
}

//actually acquire lock
lock_protocol::status 
lock_client_cache::acquire_actual(lock_protocol::lockid_t lid, lock_status_client* ls)
{
  int r;
  lock_protocol::status ret = cl->call(lock_protocol::acquire, lid, id, r);
  
  while (ret != lock_protocol::OK){
    //if a retry call arrived, status is LOCKED	
    if (is_lock_status(ls, LOCKED)){
      break;
    }
    if (ret != lock_protocol::RETRY){
      ret = cl->call(lock_protocol::acquire, lid, id, r);
    }
  }

  set_lock_status(ls, LOCKED);

  return ret;
}

//actually release lock
lock_protocol::status 
lock_client_cache::release_actual(lock_protocol::lockid_t lid, lock_status_client* ls)
{
  int r;
  lock_protocol::status ret = cl->call(lock_protocol::release, lid, id, r);

  while (ret != lock_protocol::OK){
    ret = cl->call(lock_protocol::release, lid, id, r);
  }

  set_lock_status(ls, NONE);
  pthread_cond_signal(&ls->lock_status_cv);

  return ret;
}



