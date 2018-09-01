#define __EPOLL_H__
#ifdef  __EPOLL_H__

#include "common.h"
#include "encry.h"

class IgnoreSigalPipe {
  public:
    IgnoreSigalPipe() {
      ::signal(SIGPIPE, SIG_IGN);
    }
};

static IgnoreSigalPipe initPIPE_IGN;

// 实现一个epoll服务器的类
class EpollServer {
public:
  // 构造函数和析构函数
  EpollServer(int port)
    :_port(port)
    ,_listenfd(-1)
    ,_eventfd(-1)
  {}

  virtual ~EpollServer() {
    if(_listenfd != -1) 
      close(_listenfd);
  }

  void OPEvent(int fd, int events, int op) {
    // 根据op的不同
    // ADD,DEL,MOD
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    if(epoll_ctl(_eventfd, op, fd, &ev) < 0) {
      // 打一个错误日志
      ErrorLog("epoll_ctl(op:%d, fd:%d)", op, fd);
    }
  }

  void SetNoBlock(int fd) {
    int flag = fcntl(fd, F_GETFL, 0);
    if(flag == -1) 
      ErrorLog("SetNoBlock:F_GETFL");
    flag |= O_NONBLOCK;
    int ret = fcntl(fd, F_SETFL, flag);
    if(ret == -1) {
      ErrorLog("SetNoBlock:F_SETGL");
    }
  }
  
  // 建立链接时的状态
  enum Socks5State {
    AUTH,           // 身份认证
    ESTABLISHMENT,  // 建立链接
    FORWARDING      // 转发状态
  };

  // 类似于通道的作用
  struct Channel {
    int fd;   // 描述符
    string buff; // 写缓冲

    Channel()
      :fd(-1)
    {}
  };

  struct Connect {
    Socks5State _State;       // 连接时的状态
    Channel _clientChannel;   // 保存有客户端的fd
    Channel _serverChannel;   // 服务端的fd
    int _ref;

    // 构造函数时
    // 默认的连接状态为AUTH
    Connect()
      :_State(AUTH)
      ,_ref(0)
    {}
  };

  // EpollServer中的成员函数
  void Start();
  void EventLoop();

  void SendInLoop(int fd, const char* buf, int len);
  void Forwarding(Channel* clientChannel, Channel* serverChannel,
                  bool sendencry, bool recvdecrypt);
  void RemoveConnect(int fd);

  // 多态实现的虚函数
  virtual void ConnectEventHandle(int connectfd) = 0;
  virtual void ReadEventHandle(int connectfd) = 0;
  // 这个可以不设置成虚函数
  virtual void WriteEventHandle(int connectfd);


protected:
  int _port;      // 端口
  int _listenfd;  // 监听描述符
  int _eventfd;   // 事件描述符
  
  // fd映射Connect的map
  map<int, Connect*> _fdConnectMap;
};

#endif //__EPOLL_H__

