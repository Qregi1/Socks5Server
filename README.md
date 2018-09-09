# Socks5Server

代理服务器
类图如下:
![类图](https://github.com/Qregi1/Socks5Server/blob/master/%E7%B1%BB%E5%9B%BE.png)
实现一个EpollServer类作为父类
然后再实现一个SocksServer类作为子类直接继承EpollServer类
SocksServer类处理业务:
1. 对客户端身份认证
2. 和客户端建立连接
3. 进入转发模式, 把客户端的请求转发给外网具体的服务器, 再把服务器的响应转发给请求客户端

实现一个TransferServer类.主要对数据进行加密解密
因为外网的请求可能会被墙, 实现一个加密解密之后被墙的次数很少
![流程图](https://github.com/Qregi1/Socks5Server/blob/master/%E6%B5%81%E7%A8%8B.png)

如何使用:
![插件](https://github.com/Qregi1/Socks5Server/blob/master/%E6%8F%92%E4%BB%B6.png)

项目缺点:
目前只支持ipv4协议, 对于ipv6协议暂时还不支持, 而且传输层采用TCP协议, 对于一些采用UDP协议的网站可能访问不了

