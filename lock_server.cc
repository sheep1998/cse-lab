// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

/*
 * Function Definition
 *   pthread_mutex_lock(pthread_mutex_t *mutex);
 *   pthread_mutex_unlock(pthread_mutex_t *mutex);
 *   pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
 *   pthread_cond_signal(pthread_cond_t *cond);
 *   pthread_cond_broadcast(pthread_cond_t *cond);
 */

lock_server::lock_server():
  nacquire (0)
{
  printf("lock_server : init\n");
  pthread_mutex_init(&map_mutex, NULL); //init
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("lock_server : stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab2 part2 code goes here

  printf("lock_server : acquire request from clt %d\n", clt);  
  pthread_mutex_lock(&map_mutex);
	// lid found
  if (locks.find(lid) != locks.end()) {
    while (locks[lid].granted == true) {
	pthread_cond_wait(&(locks[lid].wait), &map_mutex);
    }
  }
 	// lid not found, add lid to map
  locks[lid].granted = true;
   
  pthread_mutex_unlock(&map_mutex);

  return ret;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
	// Your lab2 part2 code goes here
 
  printf("lock_server : release request from clt %d\n", clt);
  pthread_mutex_lock(&map_mutex);
  	// lib not found
  if (locks.find(lid) != locks.end()) {
    printf("release : lid not found");
  }
  locks[lid].granted = false;
  pthread_mutex_unlock(&map_mutex);
  pthread_cond_signal(&(locks[lid].wait));

  return ret;
}



