#include "tranfer.h"

// 多态实现的虚函数
void TranferServer::ConnectEventHandle(int connectfd) {
  int serverfd = socket(AF_INET, SOCK_STREAM, 0);
  if(serverfd == -1) {
    ErrorLog("socket");
    return;
  }
  if(connect(serverfd, (struct sockaddr*)&_socks5addr, sizeof(_socks5addr)) < 0) {
    ErrorLog("connect");
    return;
  } 
  // 把connectfd设置为非阻塞并设置到epoll事件中
  SetNoBlock(connectfd);
  OPEvent(connectfd, EPOLLIN, EPOLL_CTL_ADD);

  SetNoBlock(serverfd);
  OPEvent(serverfd, EPOLLIN, EPOLL_CTL_ADD);

  Connect* con = new Connect;
  con->_State = FORWARDING;

  con->_clientChannel.fd = connectfd;
  con->_ref++;
  _fdConnectMap[connectfd] = con;

  con->_serverChannel.fd = serverfd;
  con->_ref++;
  _fdConnectMap[serverfd] = con;
}

void TranferServer::ReadEventHandle(int connectfd) {
  map<int, Connect*>::iterator it = _fdConnectMap.find(connectfd);
  if(it != _fdConnectMap.end()) {
    Connect* con = it->second;
    Channel* clientChannel = &con->_clientChannel;
    Channel* serverChannel = &con->_serverChannel;
    bool sendencry = true, recvdecrypt = false;

    if(connectfd == con->_serverChannel.fd) {
      swap(clientChannel, serverChannel);
      swap(sendencry, recvdecrypt);
    }
    Forwarding(clientChannel, serverChannel, sendencry, recvdecrypt);
  }
  else {
    ErrorLog("map error");
  }
}

int main() {
  TranferServer server(8000, "168.192.134.100", 8001);
  server.Start();
}





