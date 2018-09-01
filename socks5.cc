#include "socks5.h"

void Socks5Server::ConnectEventHandle(int connnectfd) {
  TraceLog("new connect event: %d", connnectfd);

  // 添加connectfd到epoll事件中,监听读事件
  SetNoBlock(connnectfd);
  OPEvent(connnectfd, EPOLLIN, EPOLL_CTL_ADD);

  // 创建一个连接的对象
  Connect* con = new Connect;
  // 设置这个连接的属性
  con->_State = AUTH;
  con->_clientChannel.fd = connnectfd;
  // 再把这个连接存入map中
  _fdConnectMap[connnectfd] = con;
  // 把这个ref自增
  con->_ref++;
}


/*
*  在sock5协议中,客户端连接服务器的时候,发送一个message
*  由三部分组成:
*  VER,表示协议版本,占1个字节 如果是socks5协议,值为0x05
*  NMETHODS 表示方法,占1个字节
*  METHOD   长度为1-255
*
*/
// +----+----------+----------+
// |VER | NMETHODS | METHODS  |
// +----+----------+----------+
// | 1  |    1     | 1 to 255 |
// +----+----------+----------+

/* 服务器会返回一个确认的message
* 由两部分组成
* VER 和 METHOD
* METHOD的值
*   The values currently defined for METHOD are:
*
*   o  X'00' NO AUTHENTICATION REQUIRED
*   o  X'01' GSSAPI
*   o  X'02' USERNAME/PASSWORD
*   o  X'03' to X'7F' IANA ASSIGNED
*   o  X'80' to X'FE' RESERVED FOR PRIVATE METHODS
*   o  X'FF' NO ACCEPTABLE METHODS
*/
int Socks5Server::AuthHandle(int fd) {
  char buf[260];
 // MSG_PEEK 相当于先偷看了一下
	int rlen = recv(fd, buf, 260, MSG_PEEK);
	if(rlen <= 0) {
		return -1;
	}
	else if(rlen < 3) {
		return 0;
	}
	else {
		// 表示数据接收正常
		// 由于前面已经知道发过来多少个数字了
		// 所以这里直接用数据的长度rlen
		recv(fd, buf, rlen, 0);
		// 接收到客户端的数据,进行加密
		Decrypt(buf, rlen);

		if(buf[0] != 0x05) {
		  ErrorLog("not socks5");
          return -1;
		}
    return 1;
	}
}

//
//
//  建立连接时socks5的request是这样的
//  +----+-----+-------+------+----------+----------+
//  |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
//  +----+-----+-------+------+----------+----------+
//  | 1  |  1  | X'00' |  1   | Variable |    2     |
//  +----+-----+-------+------+----------+----------+
//  VER        protocol version: X'05
//  CMD
//    o  CONNECT       X'01'
//    o  BIND          X'02'
//    o  UDP ASSOCIATE X'03'
//  RSV        RESERVED
//  ATYP       address type of following address
//    o IP V4 address:  X'01'
//    o DOMAINNAME:     X'03'  域名
//    o IP V6 address:  X'04'
//  DST.ADDR   desired destination address
//  DST.PORT   desired destination port in network octet order
//
//
// 返回的reply是这样的
// +----+-----+-------+------+----------+----------+
// |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
// +----+-----+-------+------+----------+----------+
// | 1  |  1  | X'00' |  1   | Variable |    2     |
// +----+-----+-------+------+----------+----------+
//   REP    Reply field:
//    o  X'00' succeeded
//    o  X'01' general SOCKS server failure
//    o  X'02' connection not allowed by ruleset
//    o  X'03' Network unreachable
//    o  X'04' Host unreachable
//    o  X'05' Connection refused
//    o  X'06' TTL expired
//    o  X'07' Command not supported
//    o  X'08' Address type not supported
//    o  X'09' to X'FF' unassigned
// 建立连接
// 失败返回-1
// 数据没收到返回-2
// 连接成功返回serverfd

int Socks5Server::EstablishmentHandle(int fd) {
  char buf[256];
  int rlen = recv(fd, buf, 256, MSG_PEEK);
  TraceLog("recv data:%s", buf);
  if(rlen <= 0) {
    return -1;
  }
  else if(rlen < 10) {
    return -2;
  }
  else {
    char ip[4];
    char port[2];

    // 先读取前四个字节的数据
    recv(fd, buf, 4, 0);
    Decrypt(buf, 4);

    // 取出 ATYP
    char addresstype = buf[3];
    // 判断是什么类型
    // 0x01是ipv4
    if(addresstype == 0x01) {
      // 如果是ipv4, 就保存ip地址和端口号
      // 对数据进行加密
      recv(fd, ip, 4, 0);
      Decrypt(ip, 4);

      recv(fd, port, 2, 0);
      Decrypt(port, 2);
    }
    // 0x03 是域名
    else if(addresstype == 0x03) {
      char len = 0;
      recv(fd, &len, 1, 0);
      Decrypt(&len, 1);

      recv(fd, buf, len, 0);
      buf[len] = '\0';

      TraceLog("Encry domainname:%s", buf);
      Decrypt(buf, len);

      // recv port
      recv(fd, port, 2, 0);
      Decrypt(port, 2);
      TraceLog("Decrypt domainname:%s, port:%s", buf, port);

      struct hostent* hostptr = gethostbyname(buf);
      memcpy(ip, hostptr->h_addr, hostptr->h_length);
    }
    // 0x04 是ipv6
    else if(addresstype == 0x04) {
      ErrorLog("not support ipv6");
      return -1;
    }
    else {
      ErrorLog("invalid addresstype");
      return -1;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    memcpy(&addr.sin_addr.s_addr, ip, 4);
    addr.sin_port = *((uint16_t*)port);

    int serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if(serverfd < 0) {
      ErrorLog("server socket");
      return -1;
    }

    int ret = connect(serverfd, (struct sockaddr*)&addr, sizeof(addr));
    if(ret < 0) {
      ErrorLog("connect error");
      close(serverfd);
      return -1;
    }
    else {
      return serverfd;
    }
  }
}

void Socks5Server::ReadEventHandle(int connectfd) {
  TraceLog("Read event:%d",connectfd);
  map<int, Connect*>::iterator it = _fdConnectMap.find(connectfd);
  if(it != _fdConnectMap.end()) {
    Connect* con = it->second;
    if(con->_State == AUTH) {
      // reply2是我接收到AUTH请求之后返回的数据
      // 第一个是协议socks5
      TraceLog("Ready To Auth");
      char reply[2];
      reply[0] = 0x05;
      int ret = AuthHandle(connectfd);
      if(ret == 0) {
        return;
      }
      else if(ret == 1) {
        // 0x00表示 no authentication required
        reply[1] = 0x00;
        con->_State = ESTABLISHMENT;
      }
      else if(ret == -1) {
        // 0xFF 表示不接受这种方式
        reply[1] = 0xFF;
        RemoveConnect(connectfd);
      }
      // !!!
      Encry(reply, 2);
      if(send(connectfd, reply, 2, 0) != 2) {
        ErrorLog("auth reply");
      }
    }
    else if(con->_State == ESTABLISHMENT) {
      // 建立连接
      char reply[10] = {0};
      reply[0] = 0x05;

      int serverfd = EstablishmentHandle(connectfd);
      if(serverfd == -1) {
        // 0x00表示成功, 0x01表示失败
        TraceLog("身份确认失败");
        reply[1] = 0x01;
        RemoveConnect(connectfd);
      }
      else if(serverfd == -2) {
        // 没收到数据
        TraceLog("没收到数据");
        return;
      }
      else {
        reply[1] = 0x00;
        // 0x01表示是ipv4地址
        reply[3] = 0x01;
      }
      Encry(reply, 10);
      TraceLog("身份认证");
      if(send(connectfd, reply, 10, 0) != 10) {
        ErrorLog("establishment reply");
      }
      TraceLog("serverfd:%d", serverfd);
      if(serverfd >= 0) {
        SetNoBlock(serverfd);
        OPEvent(serverfd, EPOLLIN, EPOLL_CTL_ADD);
        con->_serverChannel.fd= serverfd;
        _fdConnectMap[serverfd] = con;
        con->_ref++;
        con->_State = FORWARDING;
      }
    }
    else if(con->_State == FORWARDING) {
      Channel* clientChannel = &con->_clientChannel;
      Channel* serverChannel = &con->_serverChannel;
      // 正常情况时
      bool sendencry = false, recvdecrypt = true;

      if(connectfd == serverChannel->fd) {
        // 如果连接的fd是服务端发来的数据
        swap(clientChannel, serverChannel);
        swap(sendencry, recvdecrypt);
      }

      // client 发送给--> server
      TraceLog("Is Ready to Forwarding");
      Forwarding(clientChannel, serverChannel,
                sendencry, recvdecrypt);
    }
    else {
      // 基本不可能走到这一步
      ErrorLog("a big error");
    }
  }
  else {
    // 也不可能走到这一步
    ErrorLog("a biger error");
  }
}

int main() {
  Socks5Server server(9090);
  server.Start();
}
