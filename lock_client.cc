// RPC stubs for clients to talk to lock_server

#include "lock_client.h"
#include "rpc.h"
#include <arpa/inet.h>

#include <sstream>
#include <iostream>
#include <stdio.h>

lock_client::lock_client()
{

}

lock_client::lock_client(std::string dst)
{
  printf("lock_client: init start\n");
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  printf("lock_client: init middle\n");
  if (cl->bind() < 0) {
    printf("lock_client: call bind\n");
  }
  printf("lock_client: init end\n");
}

int
lock_client::stat(lock_protocol::lockid_t lid)
{
	// demo
  printf("lock_client: stat\n");
  int r;
  lock_protocol::status ret = cl->call(lock_protocol::stat, cl->id(), lid, r);
  VERIFY (ret == lock_protocol::OK);
  return r;
}

lock_protocol::status
lock_client::acquire(lock_protocol::lockid_t lid)
{
	// Your lab2 part2 code goes here
  printf("lock_client: acquire\n");
  int r;
  lock_protocol::status ret = cl->call(lock_protocol::acquire, cl->id(), lid, r); 
  VERIFY (ret == lock_protocol::OK);
  return r;
}

lock_protocol::status
lock_client::release(lock_protocol::lockid_t lid)
{
	// Your lab2 part2 code goes here
  printf("lock_client: release\n");
  int r;
  lock_protocol::status ret = cl->call(lock_protocol::release, cl->id(), lid, r);
  VERIFY (ret == lock_protocol::OK);
  return r;
}



