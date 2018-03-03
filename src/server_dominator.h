#ifndef IMP_SERVER_DOMINATOR_H_
#define IMP_SERVER_DOMINATOR_H_
/**
 *      imp_server_dominator.h/imp_server_dominator.cpp
 *      实现搭建与平台无关的服务端具体代码
*/

#ifdef _WIN32
#include "windows_socks_server.h"
#define Adapter nswindows
#else
#include "linux_socks_server.h"
#define Adapter nslinux
#endif
#include <time.h>
#include <ctime>
#include "auth_manager.h"


namespace NSServer{
    //  初始化服务端环境
    int DoServerInitialized(void);

	// Socks-Server
    class SocksDominator 
    {
    public:

		///
		// 创建Socks-Server服务器
		// port : 可设置该服务器的端口
		// enableAuthorized :  是否需要授权认证(默认为false)
		// defaultBufferSize :  接收缓冲区大小
		// defaultAcceptCount : 需要投递的缓存接收套接字数量
		// defaultFirstLinkTimeout : 连接后未发送数据包的超时时间(ms)
		// defaultLinkTargetTimeout :	代理连接至目标服务器的超时时间(ms)
		///
        explicit SocksDominator(int port,bool enableAuthorized = false,
			int defaultBufferSize = SOCKET_DEFAULT_BUFFER,
			int defaultAcceptCount = SOCKET_DEFAULT_ACCEPT,
			int defaultFirstLinkTimeout = SOCKET_DEFAULT_FIRST_LINK_TIMEOUT,
			int defaultLinkTargetTimeout = SOCKET_DEFAULT_LINK_TARGET_TIMEOUT);
        ~SocksDominator();

    public:
        //  启动服务
        void Run(void);

        //  停止服务
        void Stop(void);

    private:
        int m_port;
		bool m_enableAuthorized;
		int m_defaultBufferSize;
		int m_defaultAcceptCount;
		int m_defaultFirstLinkTimeout;
		int m_defaultLinkTargetTimeout;
        ref::AutoRefPtr<Adapter::SocksNetworkTransfered> m_pServer;
    };

	//获取时间戳
	__int64 GetTimeStamp(void);

	//授权信息
	extern AuthManager Auth;
}
#endif