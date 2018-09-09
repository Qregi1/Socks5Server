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
在谷歌浏览器或者火狐浏览器中下载如下的插件,插件名字:SwitchyOmega, 如图:
![插件](https://github.com/Qregi1/Socks5Server/blob/master/%E6%8F%92%E4%BB%B6.png)

通过这个插件就可以使用代理服务器了, 把HTTP, HTTPS, FTP等应用层协议的代理协议都更改为socks5协议,设置代理服务器
具体操作: 
1. 代理服务器为本机, 在本机部署transfer服务, 对浏览器的请求进行加密然后传输给一个可以访问外网的服务器(我的是阿里云在香港的云服务器)
2. 然后在可以访问外网的服务器上部署socks5服务器, 对加密的数据进行接收, 然后再把请求数据进行转发
3. 转发之后再对响应进行接收, 然后加密再转发回本地, 本地的transfer服务器再对数据进行解密再返回给浏览器
这样就可以达到访问外网的效果

项目缺点:
目前只支持ipv4协议, 对于ipv6协议暂时还不支持, 而且传输层采用TCP协议, 对于一些采用UDP协议的网站可能访问不了
故目前只能访问部分外网
