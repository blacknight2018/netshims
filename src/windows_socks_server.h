#ifndef IMP_WINDOWS_SOCKS_SERVER_H_
#define IMP_WINDOWS_SOCKS_SERVER_H_
/**
 *      imp_windows_socks_server.h/imp_windows_socks_server.cpp
 *      提供windows平台下的底层网络接口
*/
#include <WinSock2.h>
#include <mswsock.h>
#include <Windows.h>
#include "ref.h"
#include <functional>
#include <thread>
#include <vector>
#include <string>
#include "async_task.h"
#include <list>
#include <mutex>
#include <Ws2tcpip.h>
#include "auth_manager.h"

#pragma comment(lib,"ws2_32.lib")



using namespace ref;

#define HANDLE_DESTORY(x)	if(x!=NULL){CloseHandle(x);x=NULL;}

#define SOCKET_SUCESS                   (0)

//默认的缓冲区大小
#define SOCKET_DEFAULT_BUFFER           (0x8000)

//默认投递Accpet的数量
#define SOCKET_DEFAULT_ACCEPT           (64)

//默认客户首次连接至服务器后多长时间内未发送数据包的断开处理
#define SOCKET_DEFAULT_FIRST_LINK_TIMEOUT		(3000)

//连接至目标服务器的超时时间
#define SOCKET_DEFAULT_LINK_TARGET_TIMEOUT			(3000)

//Socks5协议
#ifndef SOCKS_PROTOCOL_V5_
#define SOCKS_PROTOCOL_V5_

/*
Client:
+ ---- + ---- - +------ - +------ + ---------- + ---------- +
| VER | CMD | RSV　 | ATYP | DST.ADDR | DST.PORT |
+---- + ---- - +------ - +------ + ---------- + ---------- +
| 1 | 1 | X'00' | 1 | Variable | 2 |
+---- + ---- - +------ - +------ + ---------- + ---------- +
o VER protocol version：X'05'
o CMD
　o CONNECT X'01'
 　o BIND X'02'
  　o UDP ASSOCIATE X'03'
   o RSV RESERVED
   o ATYP address type of following address
   　o IP V4 address: X'01'
	　o DOMAINNAME: X'03'
	 　o IP V6 address: X'04'
	  o DST.ADDR desired destination address
	  o DST.PORT desired destination port in network octet order
*/

/*
Server:
+----+-----+-------+------+----------+----------+
|VER | REP |　RSV　| ATYP | BND.ADDR | BND.PORT |
+----+-----+-------+------+----------+----------+
| 1　|　1　| X'00' |　1 　| Variable | 　　2　　|
+----+-----+-------+------+----------+----------+
o VER protocol version: X'05'
o REP Reply field:
　　o X'00' succeeded
  　　o X'01' general SOCKS server failure
	　　o X'02' connection not allowed by ruleset
	  　　o X'03' Network unreachable
		　　o X'04' Host unreachable
		  　　o X'05' Connection refused
			　　o X'06' TTL expired
			  　　o X'07' Command not supported
				　　o X'08' Address type not supported
				  　　o X'09' to X'FF' unassigned
					o RSV RESERVED
					o ATYP address type of following address
					　　o IP V4 address: X'01'
					  　　o DOMAINNAME: X'03'
						　　o IP V6 address: X'04'
						  o BND.ADDR server bound address
						  o BND.PORT server bound port in network octet order
						  标志RESERVED(RSV)的地方必须设置为X'00'。
*/


#define SOCKS_V5_VER							5

#define SOCKS_V5_CMD_CONNECT		1
#define SOCKS_V5_CMD_BIND				2
#define SOCKS_V5_CMD_UDP					3

#define SOCKS_V5_ATYP_IPV4					1
#define SOCKS_V5_ATYP_DOMAIN			3
#define SOCKS_V5_ATYP_IPV6					4

#define SOCKS_V5_REP_SUCCESSED		0
#define SOCKS_V5_REP_FAILED				1
#define SOCKS_V5_RSV							0


#endif

//Server类型
enum SOCKS_SERVER_TYPE {
	SST_LISTEN,		//监听
    SST_SERVER,     //转向至服务端
    SST_CLIENT      //接收至客户端的连接
};

//网络消息
enum SOCKS_NET_STATUS {
    NET_ACCEPT,
    NET_CONNECT,
    NET_SEND,
    NET_RECV,
    NET_CLOSE
};

//代理过程
enum SOCKS_PROXY_STATUS {
    SPS_NOTHING,    //未开始代理
	SPS_AUTH,			//验证
	SPS_LINK,				//开始连接目标服务器
	SPS_FINSH			//代理完成
};

//网络数据包
struct SOCKS_NET_BUFFER {
    WSAOVERLAPPED headLink;
    SOCKS_NET_STATUS status;
    BYTE *pBuffer;
    int total;
    LPVOID pHandler;
};

//客户信息
struct SOCKS_CLIENT_INFO {
    //客户连接至本服务器的IP
    SOCKADDR_IN clientLinkAddr;

    //客户要连接到的目标IP
    SOCKADDR_IN serverLinkAddr;

	//客户要连接到的目标域名
	std::string serverDomain;

    //客户登录的验证帐号和密码
    std::string user;
    std::string pwd;

    //代理步骤
    SOCKS_PROXY_STATUS nowPosition;

	//客户名称
	std::string name;

	//客户的地址类型
	BYTE ipMode;
};

//异步任务标识
enum SOCKS_ASYNC_SHELL {

	//首次连接检查
	SAS_FIRST_LINK_CHECK,

	//转向至连接至服务器
	SAS_LINK_TARGET,

	//检查是否成功连接至目标服务器
	SAS_LINK_SUCCESS,

	//主动关闭连接
	SAS_CLOSE_LINK,

	//活动连接
	SAS_KEEPALIVE,

	//域名解析任务
	SAS_DOMAIN_PARSER,

	//授权用户状态更新任务
	SAS_AUTH_UPDATE
};

namespace nswindows {

	//管理重叠IO的类
	class SockByteStream;

	//实现IO请求计数的类
	class SockRequestRefCount
	{
	public:
		SockRequestRefCount() :m_ref_count(0) {}
		~SockRequestRefCount() {}

	public:
		Atomic32 operator++(int) {
			Atomic32 r = InterlockedExchange(&m_ref_count, m_ref_count);
			InterlockedIncrement(&m_ref_count);
			return r;
		}

		Atomic32 operator--(int) {
			Atomic32 r = InterlockedExchange(&m_ref_count, m_ref_count);
			InterlockedDecrement(&m_ref_count);
			return r;
		}

		Atomic32 operator()(void) {
			return InterlockedExchange(&m_ref_count, m_ref_count);
		}

		Atomic32 operator++(void) {
			return InterlockedIncrement(&m_ref_count);
		}

		Atomic32 operator--(void) {
			return InterlockedDecrement(&m_ref_count);
		}

		bool operator==(int v) {
			return InterlockedExchange(&m_ref_count, m_ref_count) == v ? true : false;
		}

	private:
		Atomic32 m_ref_count;
	};

	//初始化
    int DoServerInitialized(void);

	//实现套接字管理的类
    class SocksNetworkTransfered : public AutoSink
    {
		friend class SockAsyncTask;
		friend class SockFirstLinkCheckTask;
		friend class SockLinkTargetTask;
		friend class SockLinkTargetExTask;
		friend class SockLinkSucessTask;
		friend class SockCloseLinkTask;
		friend class SockKeepAliveTask;
		friend class SockDomainParseTask;
		friend class SockAuthUpdateTask;
    public:
        //建立代理服务
        SocksNetworkTransfered(int port = 1080,
			BOOL enableAuthorized = FALSE,
			int defaultBufferSize = SOCKET_DEFAULT_BUFFER,
			int defaultAcceptCount = SOCKET_DEFAULT_ACCEPT,
			int defaultFirstLinkTimeout = SOCKET_DEFAULT_FIRST_LINK_TIMEOUT,
			int defaultLinkTargetTimeout = SOCKET_DEFAULT_LINK_TARGET_TIMEOUT);

		///
        // 建立转发服务
		// type : 服务类型
		// s : 绑定的套接字
		// iocpHandle : 绑定的IOCP
		// pair : 绑定的连接对 , 当一条正向连接代理建立时,会产生两个连接点L1: Client->Proxy, L2: Proxy->Server,
		// 当该对象类型为"SST_SERVER"时,表示为"L1",当该对象类型为"SST_CLIENT"时,表示为"L2",
		// "L1"接收到的数据将通过| pair | 所指向的"L2"对象发送出去,
		// 反之"L2"对象接收的数据也将通过| pair | 指向的"L1"对象发送出去。
		// server : 指向'socks-server'的监听对象,只有当| type | 为"SST_SERVER"时,该参数才可使用。
		// taskpool : 异步任务池对象,用于投递异步回调任务
		///
        SocksNetworkTransfered(SOCKS_SERVER_TYPE type,
			SOCKET s,
			HANDLE iocpHandle,
			AutoRefPtr<SocksNetworkTransfered> pair, 
			AutoRefPtr<SocksNetworkTransfered> server,
			AutoRefPtr<AsyncTaskPool> taskpool);

        ~SocksNetworkTransfered();

    private:
		//通用
		int m_errCode;
		SOCKET m_socket;
		SockRequestRefCount m_requestRef;	//IO请求计数
		AutoRefPtr<AsyncTaskPool> m_taskDispatchs;		//异步任务处理
		AutoRefPtr<AsyncTask> m_pending;	//绑定的任务
		ULONGLONG m_dataflow;	//数据流量统计,mb

		//Listen
		BOOL m_nowServerStatus;
		std::list<AutoRefPtr<SocksNetworkTransfered>> m_keepAlives;	//保存正常的连接
		std::mutex m_lockKeepAlives;
		int m_defaultBufferSize;
		int m_defaultAcceptCount;
		int m_defaultFirstLinkTimeout;
		int m_defaultLinkTargetTimeout;
		AuthManager *m_pAuth;	//授权信息
		AutoRefPtr<SockAuthUpdateTask> m_authTask;

		//Server
        int m_port;
        HANDLE m_iocpServerHandle;  //服务端完成端口
        HANDLE m_iocpClientHandle;  //代理端完成端口
		BOOL m_enableAuthhorized;	//是否启用验证授权(TRUE = 需要验证,FALSE = 无需验证)

		//Client
        AutoRefPtr<SocksNetworkTransfered> m_pair;     //指向客户连接端或者连接转向端
        AutoRefPtr<SocksNetworkTransfered> m_server;   //代理服务器
        SOCKS_SERVER_TYPE m_type;   //服务类型
        SOCKS_CLIENT_INFO m_client; //客户信息

    public:
        int IsSucessed(void){return m_errCode;}

		//设置授权数据库
		void SetAuth(AuthManager *pAuth) { m_pAuth = pAuth; };

    public:
        ULONG_PTR GetInterface() { this->AddRef(); return (ULONG_PTR)this;}

        AutoRefPtr<SocksNetworkTransfered> static ToHandler(ULONG_PTR pCompleteKey){
            AutoRefPtr<SocksNetworkTransfered> object;
            if(pCompleteKey){
                object.set(reinterpret_cast<SocksNetworkTransfered*>(pCompleteKey));
                object->Release();//需要释放一次由"GetInterface"所增加的引用计数
            }
            return object;
        }

    private:
        void static CALLBACK OnServerWorker(SocksNetworkTransfered *pLead);
        void static CALLBACK OnClientWorker(SocksNetworkTransfered *pLead);
        void OnDispatchs(AutoRefPtr<SockByteStream> request,DWORD receiveBytesTransfered);
        BOOL OnError(AutoRefPtr<SockByteStream> request,DWORD receiveBytesTransfered, int code);

        void DoAccept(AutoRefPtr<SockByteStream> request,DWORD receiveBytesTransfered);
        void DoConnected(AutoRefPtr<SockByteStream> request,DWORD receiveBytesTransfered);
        void DoReceive(AutoRefPtr<SockByteStream> request,DWORD receiveBytesTransfered);
        void DoSend(AutoRefPtr<SockByteStream> request,DWORD receiveBytesTransfered);
        void DoClose(AutoRefPtr<SockByteStream> request,DWORD receiveBytesTransfered);

    public:
		///
		// 投递一个缓存接收套接字
		///
        BOOL PostAccept(void);

		///
		//	未使用
		///
        AutoRefPtr<SocksNetworkTransfered> static PostConnected(const char* addr,int port);

		///
		// 向套接字中投递一份发送请求
		///
        BOOL PostSend(BYTE *pBuf,int len);

		///
		// 向套接字中投递一份接收请求
		///
        BOOL PostReceive(AutoRefPtr<SockByteStream> request);

		///
		// 要求关闭服务器,只有| m_type | 类型为"SST_LISTEN"时才可使用
		///
		void PostClosed(void);

		///
		// 绑定一个未完成的异步回调任务
		///
		void SetPending(AutoRefPtr<AsyncTask> pTask) { m_pending = pTask; }

		///
		// 重置已绑定的任务
		///
		void ResetPending(void) { m_pending = NULL; };

		///
		// 获取绑定的任务
		///
		AutoRefPtr<AsyncTask> GetPending(void) { return m_pending; }

		///
		// 设置连接对,
		// pair : 当一条正向连接代理建立时,会产生两个连接点L1: Client->Proxy, L2: Proxy->Server,
		// 当该对象类型为"SST_SERVER"时,表示为"L1",当该对象类型为"SST_CLIENT"时,表示为"L2",
		// "L1"接收到的数据将通过| pair | 所指向的"L2"对象发送出去,
		// 反之"L2"对象接收的数据也将通过| pair | 指向的"L1"对象发送出去。
		///
		void SetPair(AutoRefPtr<SocksNetworkTransfered> pair);

	public:
		// Proxy Method ...

		//无需授权模式
		void PostProxyFreeMode(void);

		//需要授权的模式
		void PostProxyAuthMode(void);

		//代理目标服务器成功,返回客户地址信息
		void PostProxyFinshed(void);

		//验证失败
		void PostProxyAuthFailed(void);

		//验证成功
		void PostProxyAuthFinshed(void);

		//代理目标服务器失败
		void PostProxyFailed(void);


	public:
		//接收到首次握手消息
		BOOL DoProxyNothing(AutoRefPtr<SockByteStream> request, DWORD receiveBytesTransfered);

		//接收到验证消息
		BOOL DoProxyAuth(AutoRefPtr<SockByteStream> request, DWORD receiveBytesTransfered);

		//接收到代理目标服务器消息
		BOOL DoProxyTargetLink(AutoRefPtr<SockByteStream> request, DWORD receiveBytesTransfered);

    protected:
        IMPLEMENT_NS_REFCOUNTING(SocksNetworkTransfered)
    };

	//用于实现重叠IO管理的类
    class SockByteStream : public AutoSink
    {
    public:
        explicit SockByteStream(SOCKS_NET_STATUS status,int total, SOCKET s = NULL);
        ~SockByteStream();

    public:
		// 获取"WSAOVERLAPPED"接口
        WSAOVERLAPPED *GetInterface(void) {this->AddRef();return &m_stream.headLink;}

		// 将接口转换为处理器
        AutoRefPtr<SockByteStream> static ToHandler(WSAOVERLAPPED *pubLink){
            AutoRefPtr<SockByteStream> request;
            if(pubLink){
                SOCKS_NET_BUFFER *pBuf = reinterpret_cast<SOCKS_NET_BUFFER*>(pubLink);
                if(pBuf->pHandler){
                    request.set(reinterpret_cast<SockByteStream*>(pBuf->pHandler));
                    request->Release();
                }
            }
            return request;
        }

		//获取网络消息状态
        SOCKS_NET_STATUS GetStatus(){return m_stream.status;}

		//获取缓冲区
        BYTE *GetBuffer(){return m_stream.pBuffer;}

		//获取缓冲区大小
        int GetTotal(){return m_stream.total;}

		//设置网络消息状态
        void SetStatus(SOCKS_NET_STATUS status) {m_stream.status=status;}

		//获取套接字
		SOCKET GetSocket(void) { return m_socket; }

    private:
        SOCKS_NET_BUFFER m_stream;
		SOCKET m_socket;
    protected:
        IMPLEMENT_NS_REFCOUNTING(SockByteStream)
    };

	///////////////////////////////////////////////////////////////////////////
	//  异步任务
	class SockAsyncTask : public AsyncTask
	{
	public:
		SockAsyncTask(DWORD tm, AutoRefPtr<SocksNetworkTransfered> pClientHandler, SOCKS_ASYNC_SHELL cmd) :AsyncTask() {
			m_coped = FALSE;
			m_record = tm;
			m_clientHandler = pClientHandler;
		}
		virtual ~SockAsyncTask() {
			if (m_clientHandler.get()) {
				m_clientHandler = NULL;
				m_coped = TRUE;
			}
		}

	public:
		//手动完成任务
		void Finished(void) { m_coped = TRUE; }

		//获取任务类型
		SOCKS_ASYNC_SHELL GetType(void) { return m_cmd; }

	public:
		BOOL m_coped;
		DWORD m_record;
		SOCKS_ASYNC_SHELL m_cmd;
		AutoRefPtr<SocksNetworkTransfered> m_clientHandler;
	};

	//  检查用户连接至服务器到接收第一个包的时间
	class SockFirstLinkCheckTask : public SockAsyncTask
	{
	public:
		SockFirstLinkCheckTask(DWORD tm,AutoRefPtr<SocksNetworkTransfered> pClientHandler);
		~SockFirstLinkCheckTask();
	public:
		void OnCalling() OVERRIDE;
	protected:
		IMPLEMENT_NS_REFCOUNTING(SockFirstLinkCheckTask);
	};

	//转向连接至目标服务器(IP方式)
	class SockLinkTargetTask : public SockAsyncTask
	{
	public:
		SockLinkTargetTask(DWORD tm, AutoRefPtr<SocksNetworkTransfered> pClientHandler);
		~SockLinkTargetTask();
	public:
		void OnCalling() OVERRIDE;
	protected:
		IMPLEMENT_NS_REFCOUNTING(SockLinkTargetTask);
	};

	//转向连接至目标服务器(域名方式)
	class SockLinkTargetExTask : public SockAsyncTask
	{
	public:
		SockLinkTargetExTask(DWORD tm, AutoRefPtr<SocksNetworkTransfered> pClientHandler);
		~SockLinkTargetExTask();

	public:
		void OnCalling() OVERRIDE;

	protected:
		IMPLEMENT_NS_REFCOUNTING(SockLinkTargetExTask);
	};

	//检查是否成功连接至目标服务器
	class SockLinkSucessTask : public SockAsyncTask
	{
	public:
		SockLinkSucessTask(DWORD tm, AutoRefPtr<SocksNetworkTransfered> pClientHandler);
		~SockLinkSucessTask();
	public:
		void OnCalling() OVERRIDE;
	protected:
		IMPLEMENT_NS_REFCOUNTING(SockLinkSucessTask);
	};

	//主动断开连接任务
	class SockCloseLinkTask : public SockAsyncTask
	{
	public:
		SockCloseLinkTask(DWORD tm, AutoRefPtr<SocksNetworkTransfered> pClientHandler);
		~SockCloseLinkTask();
	public:
		void OnCalling() OVERRIDE;
	protected:
		IMPLEMENT_NS_REFCOUNTING(SockCloseLinkTask);
	};

	//添加活动连接
	class SockKeepAliveTask : public SockAsyncTask
	{
	public:
		SockKeepAliveTask(DWORD tm, AutoRefPtr<SocksNetworkTransfered> pClientHandler,AutoRefPtr<SocksNetworkTransfered> pTargetHandler,BOOL cancel);
		~SockKeepAliveTask();

	public:
		void OnCalling() OVERRIDE;

	private:
		AutoRefPtr<SocksNetworkTransfered> m_pTargetHandler;
		BOOL m_cancel;
	protected:
		IMPLEMENT_NS_REFCOUNTING(SockKeepAliveTask);
	};

	//解析域名任务
	class SockDomainParseTask : public SockAsyncTask
	{
	public:
		SockDomainParseTask(DWORD tm, AutoRefPtr<SocksNetworkTransfered> pClientHandler,LPCSTR domain,int port);
		~SockDomainParseTask();
	public:
		void OnCalling(void) OVERRIDE;

	private:
		std::string m_domain;
		int m_port;

	protected:
		IMPLEMENT_NS_REFCOUNTING(SockDomainParseTask);
	};

	//更新授权用户状态任务
	class SockAuthUpdateTask : public SockAsyncTask
	{
	public:
		SockAuthUpdateTask(DWORD tm, AutoRefPtr<SocksNetworkTransfered> pClientHandler);
		~SockAuthUpdateTask();
	public:
		void OnCalling(void) OVERRIDE;
	protected:
		IMPLEMENT_NS_REFCOUNTING(SockAuthUpdateTask);
	};
}

#endif