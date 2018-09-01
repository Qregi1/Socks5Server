#ifndef __SOCK5_H__
#define __SOCK5_H__

#include "epoll.h"

class Socks5Server : public EpollServer {
public:
  Socks5Server(int port)
    :EpollServer(port)
  {}

  int AuthHandle(int fd);
  int EstablishmentHandle(int fd); 

  virtual void ConnectEventHandle(int connectfd);
  virtual void ReadEventHandle(int connectfd);

  // 不需要 WriteEventHandle 这个函数,epoll中已经实现了
};

#endif //__SOCK5_H__
