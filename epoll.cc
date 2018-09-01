#include "epoll.h"

// 服务器开始
void EpollServer::Start() {
  // 监听套接字
  _listenfd = socket(PF_INET, SOCK_STREAM, 0);
  if(_listenfd == -1) {
    ErrorLog("create socket");
    return;
  }
  // 绑定端口号
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  // 监听本机的所有网卡
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(_port);
  // 绑定端口号
  int ret = bind(_listenfd, (struct sockaddr*)&addr, sizeof(addr));
  if(ret < 0) {
    ErrorLog("bind error");
    return;
  }
  ret = listen(_listenfd, 100000);
  if(ret < 0) {
    ErrorLog("listen");
    return;
  }
  // 打个日志, 记录监听哪个端口
  TraceLog("epoll server listen: %d", _port);
  // 创建epoll事件
  _eventfd = epoll_create(100000);
  if(_eventfd < 0) {
    ErrorLog("epoll_create");
    return;
  }
  // 把listenfd添加到epoll事件中
  // 把fd设置为非阻塞
  SetNoBlock(_listenfd);
  OPEvent(_listenfd, EPOLLIN, EPOLL_CTL_ADD);

  // 然后进入事件循环
  EventLoop();
}

// 事件循环
void EpollServer::EventLoop() {
  struct epoll_event ev[100000];
  while(1) {
    // 使用epoll_wait等待
    // 等有fd从不可读变为可读,或者从不可写变为可写
    int size = epoll_wait(_eventfd, ev, 100000, 0);
    // 这里的size不可能小于等于0,因为epoll_wait是阻塞的
    // 只有等待成功只有才执行下面的代码
    for(int i = 0; i < size; ++i) {
      if(ev[i].data.fd == _listenfd) {
        struct sockaddr clientaddr;
        socklen_t len;
        int connectfd = accept(_listenfd, &clientaddr, &len);
        if(connectfd < 0) {
          ErrorLog("accept");
        }
        ConnectEventHandle(connectfd);
      }
      else if(ev[i].events & EPOLLIN) {
        // 读事件
        TraceLog("Before ReadEventHandle, fd:%d",ev[i].data.fd);
        ReadEventHandle(ev[i].data.fd);
      }
      else if(ev[i].events & EPOLLOUT){
        // 写事件
        TraceLog("Before WriteEventHandle");
        WriteEventHandle(ev[i].data.fd);
      }
      else {
        ErrorLog("event: %d", ev[i].data.fd);
      }
    }// for循环结束
  }// while循环结束
}

// 删除连接
void EpollServer::RemoveConnect(int fd) {
  // 从epoll事件列表中删除
  OPEvent(fd, 0, EPOLL_CTL_DEL);
  map<int, Connect*>::iterator it = _fdConnectMap.find(fd);
  if(it != _fdConnectMap.end()) {
    Connect* con = it->second;
    if(--con->_ref == 0) {
      delete con;
      _fdConnectMap.erase(it);
    }
  }
  else {
    ErrorLog("map error in RemoveConnect");
  }
}

// 循环发送
void EpollServer::SendInLoop(int fd, const char* buf, int len) {
  int slen = send(fd, buf, len, 0);
  // send发送数据返回的值表示已经发送了多少数据
  // 不代表对方接受了多少数据
  if(slen < 0) {
    ErrorLog("send to %d", fd);
  }
  else if(slen < len) {
    // 代表一次没有发送完
    TraceLog("recv %d bytes, send %d bytes, left %d send in loop, send to :%d", len, slen, len - slen, fd);
    map<int, Connect*>::iterator it = _fdConnectMap.find(fd);
    if(it != _fdConnectMap.end()) {
      Connect* con = it->second;
      Channel* channel = &con->_clientChannel;
      // 如果fd是服务端的fd
      if(fd == con->_serverChannel.fd) {
        channel = &con->_serverChannel;
      }
      int events = EPOLLOUT | EPOLLIN | EPOLLONESHOT;
      OPEvent(fd, events, EPOLL_CTL_MOD);
      // 把剩余的数据存在对应channel的缓冲区中
      TraceLog("add left data in channel");
      channel->buff.append(buf + slen);
    }
    else {
      ErrorLog("error in SendInLoop");
    }
  }
  else {
        TraceLog("recv data: %d bytes, send: %d bytes", len, slen);
  }
}

// 转发
void EpollServer::Forwarding(Channel* clientChannel, Channel* serverChannel,
    bool sendencry, bool recvdecrypt) {
  // 接收客户端的数据,发送给服务端
  //
  char buf[7724];
  int rlen = recv(clientChannel->fd, buf, 4096, 0);
  if(rlen < 0) {
    ErrorLog("recv: %d", clientChannel->fd);
  }
  else if(rlen == 0) {
    // 因为从客户端接收数据,如果为0,代表客户端关闭了连接
    // 我们也就向服务端关闭连接
    // 只是起一个中转的操作
    shutdown(serverChannel->fd, SHUT_WR);
    // 然后把客户端的fd从epoll事件中删除
    RemoveConnect(clientChannel->fd);
  }
  else {
    // 接收到数据了
    if(recvdecrypt) {
      // 如果是接收了数据,就加密
      Decrypt(buf, rlen);
    }
    if(sendencry) {
      // 发送数据的话,就要解密
      Encry(buf, rlen);
    }
    buf[rlen] = '\0';
    // 再循环发送
    TraceLog("Before SendInLoop");
    SendInLoop(serverChannel->fd, buf, rlen);
  }
}

void EpollServer::WriteEventHandle(int fd) {
  // 写事件处理函数
  TraceLog("WriteEventHandle");
  map<int, Connect*>::iterator it = _fdConnectMap.find(fd);
  if(it != _fdConnectMap.end()) {

    Connect* con = it->second;
    Channel* channel = &con->_clientChannel;
    if(fd == con->_serverChannel.fd) {
      channel = &con->_serverChannel;
    }
    string buff;
    // 把对应的channel的buff中的数据拿出来
    buff.swap(channel->buff);
    SendInLoop(fd, buff.c_str(), buff.size());
  }
  else {
    ErrorLog("map error in WriteEventHandle");
  }
}

