#include "server_dominator.h"

namespace NSServer {
	//授权信息
	AuthManager Auth;
}

//  初始化服务端环境
int NSServer::DoServerInitialized(){
    return Adapter::DoServerInitialized();
}


///
// 创建Socks-Server服务器
// port : 可设置该服务器的端口
// enableAuthorized :  是否需要授权认证(默认为false)
// defaultBufferSize :  接收缓冲区大小
// defaultAcceptCount : 需要投递的缓存接收套接字数量
// defaultFirstLinkTimeout : 连接后未发送数据包的超时时间(ms)
// defaultLinkTargetTimeout :	代理连接至目标服务器的超时时间(ms)
///
NSServer::SocksDominator::SocksDominator(int port, bool enableAuthorized, 
	int defaultBufferSize,
	int defaultAcceptCount,
	int defaultFirstLinkTimeout,
	int defaultLinkTargetTimeout):
	m_port(port),
	m_enableAuthorized(enableAuthorized),
	m_defaultBufferSize(defaultBufferSize),
	m_defaultAcceptCount(defaultAcceptCount),
	m_defaultFirstLinkTimeout(defaultFirstLinkTimeout),
	m_defaultLinkTargetTimeout(defaultLinkTargetTimeout)
{
   
}

NSServer::SocksDominator::~SocksDominator(){

}

//  启动服务
void NSServer::SocksDominator::Run(void){
     m_pServer = new Adapter::SocksNetworkTransfered(m_port, 
		 m_enableAuthorized,
		 m_defaultBufferSize,
		 m_defaultAcceptCount,
		 m_defaultFirstLinkTimeout,
		 m_defaultLinkTargetTimeout);
	 m_pServer->SetAuth(&Auth);
}

//  停止服务
void NSServer::SocksDominator::Stop(void){
    if(m_pServer){
        //m_pServer->PostClosed();
        m_pServer = nullptr;
		printf("sock-server end!\n");
    }

}

//获取时间戳
__int64 NSServer::GetTimeStamp(void) {
	std::chrono::time_point<std::chrono::system_clock, 
		std::chrono::milliseconds> tp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
	auto tmp = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
	return  (__int64)tmp.count();
}
