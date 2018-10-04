// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

extent_client::extent_client()
{
//  es = new extent_server(); //lab1
}

extent_client::extent_client(std::string dst)
{
  printf("extent_client: init\n");
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }
}

// a demo to show how to use RPC
extent_protocol::status
extent_client::create(uint32_t type, extent_protocol::extentid_t &id)
{
  extent_protocol::status ret = extent_protocol::OK;
  //ret = es->create(type, id); //lab1
  // Your lab2 part1 code goes here
 
  printf("extent_client: create\n");
  ret = cl->call(extent_protocol::create, type, id); 

  return ret;
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  //ret = es->get(eid, buf); //lab1
  // Your lab2 part1 code goes here

  printf("extent_client: get\n");
  ret = cl->call(extent_protocol::get, eid, buf);

  return ret;
}

extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid, 
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  //ret = es->getattr(eid, attr); //lab1
  //ret = cl->call(extent_protocol::getattr, eid, attr);

  printf("extent_client: getattr\n");
  ret = cl->call(extent_protocol::getattr, eid, attr);

  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  //int r; ret = es->put(eid, buf, r); //lab1
  // Your lab2 part1 code goes here

  printf("extent_client: put\n");
  int r;
  ret = cl->call(extent_protocol::put, eid, buf, r);

  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  //int r; ret = es->remove(eid, r); //lab1
  // Your lab2 part1 code goes here

  printf("extent_client: remove\n");
  int r;
  ret = cl->call(extent_protocol::remove, eid, r);

  return ret;
}


