netshims
=========

这是一个简单的网络代理服务端程序,虽然目前仅支持`SOCKS`第五版协议,但它目前仍在努力更新和完善中,
在不久的将来它将会实现支持更多更丰富的功能,感谢你能在此找到它,希望它能在你的学习或工作中对你有所帮助。

compile(only windows)
------------------------

下载并安装[cmake](https://cmake.org/download/),并将`cmake`的运行目录"如: d:\CMake\bin"设置到系统环境变量中,
运行cmd执行以下shell:
```Bash
cd netshims
mkdir build
cd build
cmake ..
```

###### netshims 为netshims源代码文件夹路径

shell
---------------
在终端中输入`netshims --help`将会获得详细的shell选项信息。

`-r`                            = 启动服务端程序</br>
`-socks-port <port>`            = 设置`socks-server`监听端口</br>
`-socks-enable <true/false>`    = 设置是否启用`socks-server`服务程序</br>
`-socks-auth <true/false>`      = 设置是否启用授权验证</br>
`-socks-def-buffer <size>`      = 设置`socks-server`服务程序的缓冲区大小</br>
`-socks-def-accept <count>`     = 设置`socks-server`服务程序的缓存套接字投递数量</br>
`-socks-timeout-first <time>`   = 设置客户连接后未发送数据包的超时时间(毫秒)</br>
`-socks-timeout-link <time>`    = 设置代理客户连接至目标服务器的超时时间(毫秒)</br></br>
`-auth-add <user:pwd> [-max-power <data-flow(mb)>] [-time <hour>]`</br></br>
添加一个授权用户,使用选项`-max-power`或者`-time`可以限制该用户的使用范围。</br>
示例:</br>
`netshims -auth-add admin:1234 -max-power 8000 -time 240`</br>
`netshims -auth-add test:1234 -time 24`</br></br>
`-auth-del <user>`              = 取消指定用户的授权资格(`#` = 所有用户)</br></br>
`-p <client> [-cancel]`         = 设置客户白名单(使用`IPv4`地址)</br>
`-d <client> [-cancel]`         = 设置客户黑名单(使用`IPv4`地址)</br>
如果添加了`-cancel`选项,则表示为将该客户从白名单/黑名单中移除</br>

socks-server
---------------

这是一个基于`SOCKS`第五版协议实现的代理服务端程序,现在仅支持`TCP`连接的代理。 
已经实现的功能: 
* 可选的授权验证(可使用shell创建授权用户)
* 支持`IPv4`和`Domain`连接代理

即将实现的功能: 
* 授权用户的使用流量限制
* 授权用户的使用时长限制
* 白名单客户
* 黑名单客户

将来需要实现的功能:
* `IPv6`的地址支持
* `UDP`连接代理支持


