#include "windows_socks_server.h"

namespace nswindows {
    LPFN_CONNECTEX fnConnectEx;
    LPFN_ACCEPTEX fnAcceptEx;
    LPFN_GETACCEPTEXSOCKADDRS fnGetAcceptExSockAddrs;
	LPFN_DISCONNECTEX fnDisconnectEx;
}

using namespace nswindows;

int nswindows::DoServerInitialized(void){
    WSADATA nowVer;
    if(WSAStartup(MAKEWORD(2,2),&nowVer)!=SOCKET_SUCESS){
        return WSAGetLastError();
    }

    SOCKET s=WSASocket(AF_INET,SOCK_STREAM,IPPROTO_TCP,NULL,0,WSA_FLAG_OVERLAPPED);
    if((signed int)s==SOCKET_ERROR){
        return WSAGetLastError();
    }

    GUID connectEx = WSAID_CONNECTEX,
         acceptEx = WSAID_ACCEPTEX,
         getAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS,
		disconnectEx = WSAID_DISCONNECTEX;
    DWORD receiveBytes = 0;
    if(WSAIoctl(s,SIO_GET_EXTENSION_FUNCTION_POINTER,&connectEx,sizeof(GUID),&fnConnectEx,sizeof(LPFN_CONNECTEX),&receiveBytes,nullptr,nullptr)!=SOCKET_SUCESS){
        return WSAGetLastError();
    }
    if(WSAIoctl(s,SIO_GET_EXTENSION_FUNCTION_POINTER,&acceptEx,sizeof(GUID),&fnAcceptEx,sizeof(LPFN_ACCEPTEX),&receiveBytes,nullptr,nullptr)!=SOCKET_SUCESS){
        return WSAGetLastError();
    }
    if(WSAIoctl(s,SIO_GET_EXTENSION_FUNCTION_POINTER,&getAcceptExSockAddrs,sizeof(GUID),&fnGetAcceptExSockAddrs,sizeof(LPFN_GETACCEPTEXSOCKADDRS),&receiveBytes,nullptr,nullptr)!=SOCKET_SUCESS){
        return WSAGetLastError();
    }
	if (WSAIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, &disconnectEx, sizeof(GUID), &fnDisconnectEx, sizeof(LPFN_DISCONNECTEX), &receiveBytes, nullptr, nullptr) != SOCKET_SUCESS) {
		return WSAGetLastError();
	}
    closesocket(s);
    return SOCKET_SUCESS;
}

SocksNetworkTransfered::SocksNetworkTransfered(int port, BOOL enableAuthorized, 
	int defaultBufferSize,
	int defaultAcceptCount,
	int defaultFirstLinkTimeout,
	int defaultLinkTargetTimeout):
    m_port(port),
    m_errCode(0),
	m_enableAuthhorized(enableAuthorized),
	m_nowServerStatus(FALSE),
	m_defaultBufferSize(defaultBufferSize),
	m_defaultAcceptCount(defaultAcceptCount),
	m_defaultFirstLinkTimeout(defaultFirstLinkTimeout),
	m_defaultLinkTargetTimeout(defaultLinkTargetTimeout)
{
	m_type = SST_LISTEN;
	m_pAuth = NULL;
    //服务端
    m_iocpServerHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE,0,(ULONG_PTR)nullptr,0);
    if(m_iocpServerHandle == INVALID_HANDLE_VALUE){
        m_errCode = GetLastError();
        return;
    }

    //转发端
    m_iocpClientHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE,0,(ULONG_PTR)nullptr,0);
    if(m_iocpClientHandle == INVALID_HANDLE_VALUE){
        m_errCode = GetLastError();
        return;
    }

    m_socket = WSASocket(AF_INET,SOCK_STREAM,IPPROTO_TCP,nullptr,0,WSA_FLAG_OVERLAPPED);
    if((signed int)m_socket == SOCKET_ERROR){
        m_errCode = WSAGetLastError();
        return;
    }
    SOCKADDR_IN listenAddr = {0};
    listenAddr.sin_family = AF_INET;
    listenAddr.sin_port = htons(port);
    listenAddr.sin_addr.S_un.S_addr = INADDR_ANY;
    if(bind(m_socket,(sockaddr*)&listenAddr,sizeof(SOCKADDR_IN)) == SOCKET_ERROR){
        m_errCode = WSAGetLastError();
        closesocket(m_socket);
        CloseHandle(m_iocpServerHandle);
		CloseHandle(m_iocpClientHandle);
        return;
    }
    listen(m_socket,SOMAXCONN);
    CreateIoCompletionPort((HANDLE)m_socket,m_iocpServerHandle,this->GetInterface(),0);//绑定至完成端口
    
    SYSTEM_INFO sysInfo = {0};
    GetSystemInfo(&sysInfo);
    int cpu = sysInfo.dwNumberOfProcessors *2 + 2;
	/////////////////////////////////////////////////////////////////////////////
	//  创建任务池
	m_taskDispatchs = new AsyncTaskPool(cpu, cpu * 2);
	if (m_taskDispatchs->IsSuccessed() == FALSE) {
		m_errCode = WSAGetLastError();
		closesocket(m_socket);
		CloseHandle(m_iocpServerHandle);
		CloseHandle(m_iocpClientHandle);
		return ;
	}

    /////////////////////////////////////////////////////////////////////////////
    //  创建服务端线程
    for(int i=0;i<cpu;i++){
        this->AddRef();
        CloseHandle(CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)OnServerWorker,(LPVOID)this,0,NULL));
    }
    ////  创建转发端线程  
    for(int i=0;i<cpu;i++){
        this->AddRef();
        CloseHandle(CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)OnClientWorker,(LPVOID)this,0,NULL));
    }
	printf("new socks-server(%d) ... \n",m_port);
    //投递默认接收(Accept)数
    for(int k=0;k<m_defaultAcceptCount;k++){
        if(this->PostAccept() == FALSE){
            printf("Post Accept Error: %d \n",this->m_errCode);
        }
    }
	m_nowServerStatus = TRUE;
	//if (m_enableAuthhorized) {
	//	m_authTask = new SockAuthUpdateTask(GetTickCount(), this);
	//	m_taskDispatchs->PostTask(m_authTask, 5000, 5000);
	//}
}

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
SocksNetworkTransfered::SocksNetworkTransfered(SOCKS_SERVER_TYPE type,
	SOCKET s,
	HANDLE iocpHandle,
	AutoRefPtr<SocksNetworkTransfered> pair,
	AutoRefPtr<SocksNetworkTransfered> server,
	AutoRefPtr<AsyncTaskPool> taskpool):
    m_type(type),
    m_socket(s),
    m_iocpClientHandle(iocpHandle),
    m_pair(pair),
	m_server(server),
    m_taskDispatchs(taskpool),
    m_errCode(0)
{
    m_client.nowPosition = SPS_NOTHING;
	m_enableAuthhorized = FALSE;
	printf("new SocksNetworkTransfered \n");
	m_iocpServerHandle = NULL;
}


SocksNetworkTransfered::~SocksNetworkTransfered(){
    printf("%s SocksNetworkTransfered destory! \n",m_type== SST_LISTEN?"SST_LISTEN": m_type == SST_SERVER?"SST_SERVER":"SST_CLIENT");
	if (m_iocpServerHandle) {
		HANDLE_DESTORY(m_iocpServerHandle);
		HANDLE_DESTORY(m_iocpClientHandle);
	}
}

//  服务端工作函数
void CALLBACK SocksNetworkTransfered::OnServerWorker(SocksNetworkTransfered *pLead){
    WSAOVERLAPPED *pNewResponse = nullptr;
    DWORD receiveBytesTransfered = 0;
    ULONG_PTR pCompleteKey = (ULONG_PTR)NULL;
    while(true) {
		pCompleteKey = (ULONG_PTR)NULL;
		pNewResponse = nullptr;
		receiveBytesTransfered = 0;
        if(GetQueuedCompletionStatus(pLead->m_iocpServerHandle,&receiveBytesTransfered,&pCompleteKey,&pNewResponse,INFINITE)){
            if(pCompleteKey){
                SocksNetworkTransfered::ToHandler(pCompleteKey)->OnDispatchs(SockByteStream::ToHandler(pNewResponse),receiveBytesTransfered);
            }
        }
		else {
			if (pLead->HasOneRef()) {
				break;
			}
            if(pCompleteKey){
                BOOL result =  SocksNetworkTransfered::ToHandler(pCompleteKey)->OnError(SockByteStream::ToHandler(pNewResponse),receiveBytesTransfered,WSAGetLastError());
                if(result){
                    break;
                }
			}
			else break;
        }
    }
	HANDLE_DESTORY(pLead->m_iocpClientHandle);
	HANDLE_DESTORY(pLead->m_iocpServerHandle);
    pLead->Release();
}

//  转发端工作函数
void CALLBACK SocksNetworkTransfered::OnClientWorker(SocksNetworkTransfered *pLead){
    WSAOVERLAPPED *pNewResponse = nullptr;
    DWORD receiveBytesTransfered = 0;
    ULONG_PTR pCompleteKey = (ULONG_PTR)NULL;
    while(true) {
        if(GetQueuedCompletionStatus(pLead->m_iocpClientHandle,&receiveBytesTransfered,&pCompleteKey,&pNewResponse,INFINITE)){
            if(pCompleteKey){
                SocksNetworkTransfered::ToHandler(pCompleteKey)->OnDispatchs(SockByteStream::ToHandler(pNewResponse),receiveBytesTransfered);
            }
        }
		else {
			if (pLead->HasOneRef()) {
				break;
			}
            if(pCompleteKey){
                BOOL result =  SocksNetworkTransfered::ToHandler(pCompleteKey)->OnError(SockByteStream::ToHandler(pNewResponse),receiveBytesTransfered,WSAGetLastError());
                if(result){
                    break;
                }
            }
			else break;
        }
    }
	HANDLE_DESTORY(pLead->m_iocpClientHandle);
	HANDLE_DESTORY(pLead->m_iocpServerHandle);
    pLead->Release();
}

void SocksNetworkTransfered::OnDispatchs(AutoRefPtr<SockByteStream> request,DWORD receiveBytesTransfered){
	--this->m_requestRef;

	//处理异步任务
	if (this->m_pending.get()) {
		AutoRefPtr<SockAsyncTask> pTask = static_cast<SockAsyncTask*>(this->m_pending.get());
		pTask->Finished();
		this->m_taskDispatchs->CancelTask(pTask);
		this->m_pending = NULL;
	}

	if (receiveBytesTransfered == 0) {
		if (request->GetStatus() == NET_RECV) {
			//连接断开
			printf("The target server lost %d ... \n",WSAGetLastError());
			this->DoClose(request, 0);
			return;
		}
	}

	// 检查服务是否被关闭!
	if (m_type == SST_SERVER) {
		if (m_server->m_nowServerStatus == FALSE) {
			//连接断开
			this->DoClose(request, 0);
			return;
		}
	}


    switch(request->GetStatus()){
        case SOCKS_NET_STATUS::NET_ACCEPT:
            this->DoAccept(request,receiveBytesTransfered);
        break;
        case SOCKS_NET_STATUS::NET_CONNECT:
            this->DoConnected(request,receiveBytesTransfered);
        break;
        case SOCKS_NET_STATUS::NET_RECV:
            this->DoReceive(request,receiveBytesTransfered);
        break;
        case SOCKS_NET_STATUS::NET_SEND:
            this->DoSend(request,receiveBytesTransfered);
        break;
        case SOCKS_NET_STATUS::NET_CLOSE:
            this->DoClose(request,receiveBytesTransfered);
        break;   
        default:
        break;     
    }
}

BOOL SocksNetworkTransfered::OnError(AutoRefPtr<SockByteStream> request,DWORD receiveBytesTransfered,int code){
	--this->m_requestRef;

	printf("Type: %d Found Error: %d! \n", m_type,code);

	if (request->GetStatus() == SOCKS_NET_STATUS::NET_ACCEPT) {
		printf("AcceptEx %d Closed!\n", request->GetSocket());
		closesocket(request->GetSocket());	//关闭由ACCEPTEX投递出来的套接字
	}

	if (code == ERROR_CONNECTION_REFUSED) {
		if (m_type == SST_CLIENT) {
			//连接目标服务器失败

			//处理异步任务
			if (m_pair.get()) {

				printf("The client %s proxy target %s:%d failed (req: link closed)! \n", m_pair->m_client.name.c_str(),
					inet_ntoa(m_pair->m_client.serverLinkAddr.sin_addr),
					m_pair->m_client.serverLinkAddr.sin_port);

				if (m_pair->m_pending.get()) {
					AutoRefPtr<SockAsyncTask> pTask = static_cast<SockAsyncTask*>(m_pair->m_pending.get());
					pTask->Finished();
					m_pair->m_taskDispatchs->CancelTask(pTask);
					m_pair->m_pending = NULL;
				}
				m_pair->PostProxyFailed();

				//一秒后关闭连接
				AutoRefPtr<SockCloseLinkTask> pTask = new SockCloseLinkTask(GetTickCount(), m_pair);
				m_pair->m_taskDispatchs->PostTask(pTask, 1000);
			}
		}
	}
	else {
		this->DoClose(request, receiveBytesTransfered);
	}

	if (code == WSA_OPERATION_ABORTED || code == WSA_INVALID_HANDLE) {
		//由应用程序提交的CancelIo请求。
		if (m_requestRef == 0) {	//已经没有IO请求了
			return TRUE;
		}
	}

    return FALSE;
}

void SocksNetworkTransfered::DoAccept(AutoRefPtr<SockByteStream> request,DWORD receiveBytesTransfered){
	char szLocalBuffer[100];
	if (this->PostAccept() == FALSE) {
		printf("Post AcceptEx Failed!, Error: %d \n", WSAGetLastError());
	}
	setsockopt(request->GetSocket(), SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT, (char*)&m_socket, sizeof(SOCKET));

    SOCKADDR_IN *pLocalAddr = nullptr;
	SOCKADDR_IN *pRemoteAddr = nullptr;
	int localAddrLength = 0;
	int remoteAddrLength = 0;
    fnGetAcceptExSockAddrs(request->GetBuffer(), 0,
     sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,
      (sockaddr**)&pLocalAddr, &localAddrLength, (sockaddr**)&pRemoteAddr, &remoteAddrLength);
	 AutoRefPtr<SocksNetworkTransfered> pClientHandler = new SocksNetworkTransfered(SST_SERVER, request->GetSocket(), this->m_iocpServerHandle, nullptr,this, this->m_taskDispatchs);
	 CreateIoCompletionPort((HANDLE)request->GetSocket(), this->m_iocpServerHandle, pClientHandler->GetInterface(), 0);		//绑定至IOCP,销毁时需要增加一次Release
	 sprintf_s(szLocalBuffer, 100, "%s:%d", inet_ntoa(pRemoteAddr->sin_addr), htons(pRemoteAddr->sin_port));
	 printf("New Client %s Linked ... \n", szLocalBuffer);
	 pClientHandler->m_client.clientLinkAddr.sin_port = htons(pRemoteAddr->sin_port);
	 pClientHandler->m_client.clientLinkAddr.sin_addr.S_un.S_addr = pRemoteAddr->sin_addr.S_un.S_addr;
	 pClientHandler->m_client.name = szLocalBuffer;
     request->SetStatus(NET_RECV);
	 pClientHandler->PostReceive(request);
	 AutoRefPtr<SockFirstLinkCheckTask> pTask = new SockFirstLinkCheckTask(GetTickCount(), pClientHandler);//检查用户是否连接后是否发送数据
	 pClientHandler->SetPending(pTask);
	 m_taskDispatchs->PostTask(pTask, m_defaultFirstLinkTimeout);

	 AutoRefPtr<SockKeepAliveTask> pKeepAlive = new SockKeepAliveTask(GetTickCount(),this, pClientHandler,FALSE);//加入活动连接列表
	 this->m_taskDispatchs->PostTask(pKeepAlive);
}

void SocksNetworkTransfered::DoConnected(AutoRefPtr<SockByteStream> request,DWORD receiveBytesTransfered){

	if (m_pair.get()) {
		m_pair->m_client.nowPosition = SPS_FINSH;
		m_pair->PostProxyFinshed();
		//处理异步任务
		if (m_pair->m_pending.get()) {
			AutoRefPtr<SockAsyncTask> pTask = static_cast<SockAsyncTask*>(m_pair->m_pending.get());
			pTask->Finished();
			m_pair->m_taskDispatchs->CancelTask(pTask);
			m_pair->m_pending = NULL;
		}
	}

	request->SetStatus(NET_RECV);
	this->PostReceive(request);
}

void SocksNetworkTransfered::DoReceive(AutoRefPtr<SockByteStream> request,DWORD receiveBytesTransfered){
	//printf("%d receive data %d\n", m_type,receiveBytesTransfered);
	BOOL result = TRUE;
	if (m_type == SST_SERVER && this->m_client.nowPosition != SOCKS_PROXY_STATUS::SPS_FINSH) {
		switch (this->m_client.nowPosition)
		{
		case SOCKS_PROXY_STATUS::SPS_NOTHING:
			result = this->DoProxyNothing(request, receiveBytesTransfered);
			break;
		case SOCKS_PROXY_STATUS::SPS_LINK:
			result = this->DoProxyTargetLink(request, receiveBytesTransfered);
			break;
		case SOCKS_PROXY_STATUS::SPS_AUTH:
			result = SocksNetworkTransfered::DoProxyAuth(request, receiveBytesTransfered);
			break;
		default:
			result = FALSE;
			break;
		}
	}
	else  {
		m_pair->PostSend(request->GetBuffer(), receiveBytesTransfered);
		m_dataflow += receiveBytesTransfered;
	}

	if (result) {
		//继续接收数据
		request->SetStatus(NET_RECV);
		this->PostReceive(request);
	}
	else {
		this->DoClose(request, receiveBytesTransfered);
	}
}

void SocksNetworkTransfered::DoSend(AutoRefPtr<SockByteStream> request,DWORD receiveBytesTransfered){

}

void SocksNetworkTransfered::DoClose(AutoRefPtr<SockByteStream> request,DWORD receiveBytesTransfered){
	if (m_pair.get()) {
		CancelIo((HANDLE)m_pair->m_socket);
		closesocket(m_pair->m_socket);
	}

	if (m_type == SST_SERVER) {
		AutoRefPtr<SockKeepAliveTask> pKeepAlive = new SockKeepAliveTask(GetTickCount(), this->m_server, this, TRUE);//删除活动连接列表
		this->m_taskDispatchs->PostTask(pKeepAlive);
	}

	m_pair = NULL;
	m_taskDispatchs = NULL;
	m_pending = NULL;
	m_server = NULL;

	if (m_socket) {
		closesocket(m_socket);	//关闭套接字
		if (m_type==SST_LISTEN || m_type == SST_SERVER) {
			this->Release();	//套接字绑定到完成端口时使用了一次GetInterface();关闭时在这里释放一次
		}
		m_socket = NULL;
	}
}

///
// 投递一个缓存接收套接字
///
BOOL SocksNetworkTransfered::PostAccept(void){
    SOCKET s = WSASocket(AF_INET,SOCK_STREAM,IPPROTO_TCP,0,0,WSA_FLAG_OVERLAPPED);
    AutoRefPtr<SockByteStream> pConnBuf = new SockByteStream(NET_ACCEPT,m_defaultBufferSize,s);
    DWORD receiveBytesTransfered = 0;
    if(FALSE == fnAcceptEx(m_socket,s,pConnBuf->GetBuffer(),0, sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16,&receiveBytesTransfered,pConnBuf->GetInterface())){
       if(WSAGetLastError()!=WSA_IO_PENDING){
            closesocket(s);
            pConnBuf->Release();
            m_errCode = WSAGetLastError();
            return FALSE;
       }
    }
	this->AddRef();
    m_errCode = 0;
	++this->m_requestRef;
    return TRUE;
}

AutoRefPtr<SocksNetworkTransfered> SocksNetworkTransfered::PostConnected(const char* addr,int port){
	AutoRefPtr<SocksNetworkTransfered> connect;
	return connect;
}

///
// 向套接字中投递一份发送请求
///
BOOL SocksNetworkTransfered::PostSend(BYTE *pBuf,int len){
	if (m_socket) {
		DWORD bytesSent = 0;
		WSABUF data;
		data.buf = (char*)pBuf;
		data.len = len;
		if (WSASend(m_socket, &data, 1, &bytesSent, 0, NULL, NULL) == SOCKET_ERROR) {
			if (WSAGetLastError() != WSA_IO_PENDING) {
				printf("Post WSASend failed error: %d \n", WSAGetLastError());
				return FALSE;
			}
		}
		return TRUE;
	}
	return FALSE;
}

///
// 向套接字中投递一份接收请求
///
BOOL SocksNetworkTransfered::PostReceive(AutoRefPtr<SockByteStream> request){
    if(!m_socket){
        return FALSE;
    }
    DWORD flag = 0;
    DWORD receiveBytesTransfered = 0;
    WSABUF receiveBuf;
    receiveBuf.buf = (char*)request->GetBuffer();
    receiveBuf.len = request->GetTotal();
    if(WSARecv(m_socket,&receiveBuf,1,&receiveBytesTransfered,&flag,request->GetInterface(),nullptr) == SOCKET_ERROR){
        if(WSAGetLastError()!=WSA_IO_PENDING){
            printf("Post WSARecv failed error: %d \n", WSAGetLastError());
            this->DoClose(request,0);
            return FALSE;
        }
    }
	this->AddRef();
	++this->m_requestRef;
    return TRUE;
}

///
// 要求关闭服务器,只有| m_type | 类型为"SST_LISTEN"时才可使用
///
void SocksNetworkTransfered::PostClosed(void) {
	if (m_type != SST_LISTEN) return;
	m_nowServerStatus = FALSE;
	m_lockKeepAlives.lock();
	for (auto &v:m_keepAlives)
	{
		CancelIo((HANDLE)v->m_socket);
		closesocket(v->m_socket);
	}
	m_keepAlives.clear();
	m_lockKeepAlives.unlock();

	//两秒后关闭连接
	AutoRefPtr<SockCloseLinkTask> pTask = new SockCloseLinkTask(GetTickCount(), this);
	this->m_taskDispatchs->PostTask(pTask, 2000);
	this->m_taskDispatchs->CancelTask(m_authTask);
	m_authTask = NULL;
}

///
// 设置连接对,
// pair : 当一条正向连接代理建立时,会产生两个连接点L1: Client->Proxy, L2: Proxy->Server,
// 当该对象类型为"SST_SERVER"时,表示为"L1",当该对象类型为"SST_CLIENT"时,表示为"L2",
// "L1"接收到的数据将通过| pair | 所指向的"L2"对象发送出去,
// 反之"L2"对象接收的数据也将通过| pair | 指向的"L1"对象发送出去。
///
void SocksNetworkTransfered::SetPair(AutoRefPtr<SocksNetworkTransfered> pair) {
	m_pair = pair;
}

BOOL SocksNetworkTransfered::DoProxyNothing(AutoRefPtr<SockByteStream> request, DWORD receiveBytesTransfered) {
	if (receiveBytesTransfered < 3) {
		//握手协议数据长度不足
		goto jmp_error;
	}
	else {
		BYTE *protocolBuffer = request->GetBuffer();

		BYTE ver = protocolBuffer[0],
			method = protocolBuffer[1],
			req = protocolBuffer[2];

		if (ver != SOCKS_V5_VER) {
			goto jmp_error;
		}

		if (method == SOCKS_V5_CMD_CONNECT) {

			if (req == 0) {	//客户端请求无需认证的连接
				if (m_server->m_enableAuthhorized) {
					//需要授权验证
					this->PostProxyAuthMode();
					m_client.nowPosition = SPS_AUTH;
				}
				else {
					//无需授权验证
					this->PostProxyFreeMode();
					m_client.nowPosition = SPS_LINK;
				}
			}
			else if (req == 1) {	 //GSSAPI
				goto jmp_error;	//暂不支持
			}
			else if (req == 2) {	 //客户端请求需要认证的连接
				//如果客户请求认证,不管是否开启认证,均按认证处理
				this->PostProxyAuthMode();
				m_client.nowPosition = SPS_AUTH;
			}
			else {	//其它协议不支持
				goto jmp_error;	//暂不支持
			}
			return TRUE;
		}
	}

jmp_error:
	printf("The client %s use protocol version non support (req: link closed)!\n", m_client.name.c_str());
	CancelIo((HANDLE)this->m_socket);
	closesocket(this->m_socket);
	return FALSE;
}

//接收到验证消息
BOOL SocksNetworkTransfered::DoProxyAuth(AutoRefPtr<SockByteStream> request, DWORD receiveBytesTransfered) {
	BYTE *protocolBuffer = request->GetBuffer();
	BYTE ver = protocolBuffer[0];
	BYTE p = 1;
	std::string user, pwd;
	if (m_server->m_enableAuthhorized == FALSE) {
		return TRUE;
	}
	if (m_server->m_pAuth == NULL) {
		return FALSE;
	}

	if (ver == 1 || ver == SOCKS_V5_VER) {
		BYTE len = protocolBuffer[p++];
		if (p >= receiveBytesTransfered || (p+len)>= receiveBytesTransfered) {
			goto jmp_error;
		}
		user.assign((char*)&protocolBuffer[p], len);
		p += len;
		if (p >= receiveBytesTransfered) {
			goto jmp_error;
		}
		len = protocolBuffer[p++];
		if (p >= receiveBytesTransfered || (p + len) > receiveBytesTransfered) {
			goto jmp_error;
		}
		pwd.assign((char*)&protocolBuffer[p], len);
		if (m_server->m_pAuth->MatchUser(user.c_str(),pwd.c_str())) {
			this->PostProxyAuthFinshed();
			m_client.nowPosition = SPS_LINK;
			m_client.user = user;
			m_client.pwd = pwd;
		}
		else {
			this->PostProxyAuthFailed();
		}
		return TRUE;
	}
jmp_error:
	printf("The client %s use protocol version non support (req: link closed)!\n", m_client.name.c_str());
	CancelIo((HANDLE)this->m_socket);
	closesocket(this->m_socket);
	return FALSE;
}

BOOL SocksNetworkTransfered::DoProxyTargetLink(AutoRefPtr<SockByteStream> request, DWORD receiveBytesTransfered) {
	if (receiveBytesTransfered < 4) {
		goto jmp_error;
	}

	BYTE *protocolBuffer = request->GetBuffer();

	BYTE ver = protocolBuffer[0],
		method = protocolBuffer[1],
		rsv = protocolBuffer[2],
		atyp = protocolBuffer[3];

	if (ver != SOCKS_V5_VER || rsv!=NULL) {
		goto jmp_error;
	}

	if (method == SOCKS_V5_CMD_CONNECT) {

		if (atyp == SOCKS_V5_ATYP_IPV4) {
			// TCP 连接
			if (receiveBytesTransfered < 10) {
				//数据包长度不足
				goto jmp_error;
			}
			BYTE *pWriteAddr = (BYTE*)&m_client.serverLinkAddr.sin_addr.S_un.S_addr;
			pWriteAddr[0] = protocolBuffer[4];
			pWriteAddr[1] = protocolBuffer[5];
			pWriteAddr[2] = protocolBuffer[6];
			pWriteAddr[3] = protocolBuffer[7];
			m_client.serverLinkAddr.sin_port = (USHORT)(protocolBuffer[8] * 256 + protocolBuffer[9]);
			printf("The client %s proxy to target %s:%d \n", m_client.name.c_str(), inet_ntoa(m_client.serverLinkAddr.sin_addr), m_client.serverLinkAddr.sin_port);
			
			m_client.ipMode = atyp;
			//开始尝试连接至目标服务器
			AutoRefPtr<SockLinkTargetTask> pTask = new SockLinkTargetTask(GetTickCount(), this);
			this->m_taskDispatchs->PostTask(pTask);

			return TRUE;
		}
		else if (atyp == SOCKS_V5_ATYP_DOMAIN) {

			//Domain
			int length = protocolBuffer[4];
			std::string domain;
			int port;
			m_client.serverDomain.assign((char*)&protocolBuffer[5], length);
			port = *(WORD*)&protocolBuffer[5 + length];
			m_client.serverLinkAddr.sin_addr.S_un.S_addr = SOCKET_SUCESS;
			m_client.serverLinkAddr.sin_port = htons(port);
			
			printf("The client %s proxy to target %s:%d \n", m_client.name.c_str(), m_client.serverDomain.c_str(), m_client.serverLinkAddr.sin_port);

			m_client.ipMode = atyp;
			//开始尝试连接至目标服务器
			AutoRefPtr<SockDomainParseTask> pTask = new SockDomainParseTask(GetTickCount(), this, m_client.serverDomain.c_str(), m_client.serverLinkAddr.sin_port);
			this->m_taskDispatchs->PostTask(pTask);
			//AutoRefPtr<SockLinkTargetExTask> pTask = new SockLinkTargetExTask(GetTickCount(), this);
			//this->m_taskDispatchs->PostTask(pTask);
			return TRUE;
		}

	}

jmp_error:
	printf("The client %s use protocol version non support (req: link closed)!\n", m_client.name.c_str());
	CancelIo((HANDLE)this->m_socket);
	closesocket(this->m_socket);
	return FALSE;
}

//无需授权模式
void SocksNetworkTransfered::PostProxyFreeMode(void) {
	BYTE protocolBuffer[2] = { SOCKS_V5_VER , SOCKS_V5_REP_SUCCESSED };
	this->PostSend(protocolBuffer, 2);
}

//需要授权的模式
void SocksNetworkTransfered::PostProxyAuthMode(void) {
	BYTE protocolBuffer[2] = { SOCKS_V5_VER , 2 };
	this->PostSend(protocolBuffer, 2);
}

//代理目标服务器成功,返回客户地址信息
void SocksNetworkTransfered::PostProxyFinshed(void) {
	BYTE protocolBuffer[10];
	const BYTE *pReadAddr = (BYTE*)&m_client.clientLinkAddr.sin_addr.S_un.S_addr;

	protocolBuffer[0] = SOCKS_V5_VER;
	protocolBuffer[1] = SOCKS_V5_REP_SUCCESSED;
	protocolBuffer[2] = SOCKS_V5_RSV;
	protocolBuffer[3] = SOCKS_V5_ATYP_IPV4;
	protocolBuffer[4] = pReadAddr[0];
	protocolBuffer[5] = pReadAddr[1];
	protocolBuffer[6] = pReadAddr[2];
	protocolBuffer[7] = pReadAddr[3];
	protocolBuffer[8] = m_client.clientLinkAddr.sin_port / 256;
	protocolBuffer[9] = m_client.clientLinkAddr.sin_port % 256;
	this->PostSend(protocolBuffer, 10);
}

//验证失败
void SocksNetworkTransfered::PostProxyAuthFailed(void) {
	BYTE protocolBuffer[2] = { 1,SOCKS_V5_REP_FAILED };
	this->PostSend(protocolBuffer, 2);
}

//验证成功
void SocksNetworkTransfered::PostProxyAuthFinshed(void) {
	BYTE protocolBuffer[2] = { 1,SOCKS_V5_REP_SUCCESSED };
	this->PostSend(protocolBuffer, 2);
}

//代理目标服务器失败
void SocksNetworkTransfered::PostProxyFailed(void) {
	BYTE protocolBuffer[10];
	const BYTE *pReadAddr = (BYTE*)&m_client.clientLinkAddr.sin_addr.S_un.S_addr;

	protocolBuffer[0] = SOCKS_V5_VER;
	protocolBuffer[1] = SOCKS_V5_REP_FAILED;
	protocolBuffer[2] = SOCKS_V5_RSV;
	protocolBuffer[3] = SOCKS_V5_ATYP_IPV4;
	protocolBuffer[4] = pReadAddr[0];
	protocolBuffer[5] = pReadAddr[1];
	protocolBuffer[6] = pReadAddr[2];
	protocolBuffer[7] = pReadAddr[3];
	protocolBuffer[8] = m_client.clientLinkAddr.sin_port / 256;
	protocolBuffer[9] = m_client.clientLinkAddr.sin_port % 256;
	this->PostSend(protocolBuffer, 10);
}

SockByteStream::SockByteStream(SOCKS_NET_STATUS status,int total, SOCKET s):m_socket(s){
    ZeroMemory(&m_stream.headLink,sizeof(WSAOVERLAPPED));
    m_stream.pBuffer = nullptr;
    m_stream.total = total;
	m_stream.status = status;
    if(total>0){
        m_stream.pBuffer = (BYTE*)VirtualAlloc(NULL,total,MEM_COMMIT,PAGE_READWRITE);
    }
    m_stream.pHandler = this;
	
}

SockByteStream::~SockByteStream(){
    if(m_stream.pBuffer){
       VirtualFree(m_stream.pBuffer,0,MEM_RELEASE);
    }
	//printf("SockByteStream destory!\n");
}

SockFirstLinkCheckTask::SockFirstLinkCheckTask(DWORD tm, AutoRefPtr<SocksNetworkTransfered> pClientHandler):
	SockAsyncTask(tm, pClientHandler, SOCKS_ASYNC_SHELL::SAS_FIRST_LINK_CHECK){

}

SockFirstLinkCheckTask::~SockFirstLinkCheckTask() {
	printf("SockFirstLinkCheckTask destory!\n");
}

void SockFirstLinkCheckTask::OnCalling() {
	if (m_coped == FALSE) {	//任务未被处理
		printf("The client %s long time non sent data (req: link closed)! \n", m_clientHandler->m_client.name.c_str());
		CancelIo((HANDLE)m_clientHandler->m_socket);
		closesocket(m_clientHandler->m_socket);
	}
	m_coped = TRUE;
	m_clientHandler = NULL;
}

SockLinkTargetTask::SockLinkTargetTask(DWORD tm, AutoRefPtr<SocksNetworkTransfered> pClientHandler):
	SockAsyncTask(tm, pClientHandler, SOCKS_ASYNC_SHELL::SAS_LINK_TARGET) {

}

SockLinkTargetTask::~SockLinkTargetTask() {
	printf("SockLinkTargetTask destory!\n");
}

void SockLinkTargetTask::OnCalling() {

	if (m_coped == FALSE) {	//任务未被处理

		SOCKET s = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);

		SOCKET_ADDRESS_LIST listInfo = { 0 };
		SOCKADDR_IN  targetAddr = { 0 };
		listInfo.iAddressCount = 1;
		listInfo.Address[0].lpSockaddr = (sockaddr*)&targetAddr;
		targetAddr.sin_family = AF_INET;
		targetAddr.sin_addr.S_un.S_addr = m_clientHandler->m_client.serverLinkAddr.sin_addr.S_un.S_addr;
		targetAddr.sin_port = htons(m_clientHandler->m_client.serverLinkAddr.sin_port);
		DWORD dwLength1 = sizeof(SOCKADDR_IN);
		DWORD dwLength2 = sizeof(SOCKADDR_IN);
		SOCKADDR_IN outLocalAddr = { 0 };
		SOCKADDR_IN outRemoteAddr = { 0 };
		timeval timeout = { 0 };
		timeout.tv_usec = m_clientHandler->m_server->m_defaultLinkTargetTimeout;
		BOOL status = WSAConnectByList(s, &listInfo, &dwLength1, (sockaddr*)&outLocalAddr, &dwLength2, (sockaddr*)&outRemoteAddr, &timeout, NULL);
		if (!status) {
			printf("The server connected to the customer's %s needs failed (req: link closed)! \n",m_clientHandler->m_client.name.c_str());
			CancelIo((HANDLE)m_clientHandler->m_socket);
			closesocket(m_clientHandler->m_socket);
		}
		else {
			setsockopt(s, SOL_SOCKET,
				SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
			AutoRefPtr<SocksNetworkTransfered> pNewLinkHandler = new SocksNetworkTransfered(SST_CLIENT, s,
						m_clientHandler->m_server->m_iocpClientHandle,
						m_clientHandler,
						m_clientHandler->m_server,
						m_clientHandler->m_taskDispatchs);
			CreateIoCompletionPort((HANDLE)s, pNewLinkHandler->m_iocpClientHandle, pNewLinkHandler->GetInterface(), 0);	//绑定完成端口
			AutoRefPtr<SockByteStream> request = new SockByteStream(NET_CONNECT, m_clientHandler->m_server->m_defaultBufferSize);
			m_clientHandler->SetPair(pNewLinkHandler);
			pNewLinkHandler->DoConnected(request, 0);
			pNewLinkHandler->Release();
		}


		//if ((signed int)s == SOCKET_ERROR) {
		//	printf("create socket failed! (req: link closed)! \n");
		//jmp_error:
		//	CancelIo((HANDLE)m_clientHandler->m_socket);
		//	closesocket(m_clientHandler->m_socket);
		//	m_coped = TRUE;
		//	m_clientHandler = NULL;
		//	return;
		//}

		//SOCKADDR_IN connAddr = { 0 };
		//SOCKADDR_IN binAddr = { 0 };
		//connAddr.sin_family =  AF_INET;
		//binAddr.sin_family = AF_INET;
		//connAddr.sin_addr.S_un.S_addr = m_clientHandler->m_client.serverLinkAddr.sin_addr.S_un.S_addr;
		//connAddr.sin_port = htons(m_clientHandler->m_client.serverLinkAddr.sin_port);

		//if (bind(s, (sockaddr*)&binAddr, sizeof(sockaddr)) != SOCKET_ERROR) {
		//	AutoRefPtr<SocksNetworkTransfered> pNewLinkHandler = new SocksNetworkTransfered(SST_CLIENT, s,
		//		m_clientHandler->m_server->m_iocpClientHandle,
		//		m_clientHandler,
		//		m_clientHandler->m_server,
		//		m_clientHandler->m_taskDispatchs);
		//	AutoRefPtr<SockByteStream> pConnect = new SockByteStream(SOCKS_NET_STATUS::NET_CONNECT);
		//	CreateIoCompletionPort((HANDLE)s, pNewLinkHandler->m_iocpClientHandle, pNewLinkHandler->GetInterface(), 0);	//绑定完成端口
		//	DWORD sentBytes = 0;
		//	if (fnConnectEx(s, (sockaddr*)&connAddr, sizeof(sockaddr), NULL, 0, NULL, pConnect->GetInterface()) == SOCKET_ERROR) {
		//		if (WSAGetLastError() != WSA_IO_PENDING) {
		//			printf("Post ConnectEx failed! (req: link closed)! %d \n",WSAGetLastError());
		//			closesocket(s);
		//			pConnect->Release();
		//			pNewLinkHandler->Release();
		//			goto jmp_error;
		//		}
		//	}
		//	setsockopt(s, SOL_SOCKET, SO_UPDATE_CONNECT_CONTEXT, NULL, 0);

		//	m_clientHandler->SetPair(pNewLinkHandler);
		//	//是否连接成功检查
		//	AutoRefPtr<SockLinkSucessTask> pTask = new SockLinkSucessTask(GetTickCount(), m_clientHandler);
		//	m_clientHandler->SetPending(pTask);
		//	m_clientHandler->m_taskDispatchs->PostTask(pTask, SOCKET_DEFAULT_LINK_TARGET_TIMEOUT);
		//}
		//else {
		//	printf("The client %s bind failed,error %d (req: link closed)! \n", m_clientHandler->m_client.name.c_str(),WSAGetLastError());
		//	CancelIo((HANDLE)m_clientHandler->m_socket);
		//	closesocket(m_clientHandler->m_socket);
		//}
	}
	m_coped = TRUE;
	m_clientHandler = NULL;
}

SockLinkTargetExTask::SockLinkTargetExTask(DWORD tm, AutoRefPtr<SocksNetworkTransfered> pClientHandler):
	SockAsyncTask(tm, pClientHandler, SOCKS_ASYNC_SHELL::SAS_LINK_TARGET) {

}

SockLinkTargetExTask::~SockLinkTargetExTask() {

}

void SockLinkTargetExTask::OnCalling() {
	if (m_coped == FALSE) {
		SOCKET s = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		char szServiceBuffer[32];
		_itoa_s(m_clientHandler->m_client.serverLinkAddr.sin_port, szServiceBuffer,32, 10);
		int ipv6only = 0;
		setsockopt(s, IPPROTO_IPV6,
			IPV6_V6ONLY, (char*)&ipv6only, sizeof(ipv6only));

		DWORD dwLength1 = sizeof(SOCKADDR_IN);
		DWORD dwLength2 = sizeof(SOCKADDR_IN);
		SOCKADDR_IN outLocalAddr = { 0 };
		SOCKADDR_IN outRemoteAddr = { 0 };
		timeval timeout = { 0 };
		timeout.tv_usec = m_clientHandler->m_server->m_defaultLinkTargetTimeout;
		BOOL status = WSAConnectByName(s,m_clientHandler->m_client.serverDomain.c_str(), szServiceBuffer,
			&dwLength1, (sockaddr*)&outLocalAddr, &dwLength2, (sockaddr*)&outRemoteAddr, &timeout, NULL);
		if (!status) {
			printf("The server connected to the customer's %s needs failed (req: link closed)! \n", m_clientHandler->m_client.name.c_str());
			CancelIo((HANDLE)m_clientHandler->m_socket);
			closesocket(m_clientHandler->m_socket);
		}
		else {
			setsockopt(s, SOL_SOCKET,
				SO_UPDATE_CONNECT_CONTEXT, NULL, 0);
			printf("The client %s proxy to target %s:%d \n", m_clientHandler->m_client.name.c_str(), inet_ntoa(outRemoteAddr.sin_addr), htons(outRemoteAddr.sin_port));

			AutoRefPtr<SocksNetworkTransfered> pNewLinkHandler = new SocksNetworkTransfered(SST_CLIENT, s,
				m_clientHandler->m_server->m_iocpClientHandle,
				m_clientHandler,
				m_clientHandler->m_server,
				m_clientHandler->m_taskDispatchs);
			CreateIoCompletionPort((HANDLE)s, pNewLinkHandler->m_iocpClientHandle, pNewLinkHandler->GetInterface(), 0);	//绑定完成端口
			AutoRefPtr<SockByteStream> request = new SockByteStream(NET_CONNECT, m_clientHandler->m_server->m_defaultBufferSize);
			m_clientHandler->SetPair(pNewLinkHandler);
			pNewLinkHandler->DoConnected(request, 0);
			pNewLinkHandler->Release();
		}
	}
	m_coped = TRUE;
	m_clientHandler = NULL;
}

SockLinkSucessTask::SockLinkSucessTask(DWORD tm, AutoRefPtr<SocksNetworkTransfered> pClientHandler):
	SockAsyncTask(tm,pClientHandler,SAS_LINK_SUCCESS) {

}

SockLinkSucessTask::~SockLinkSucessTask() {
	printf("SockLinkSucessTask destory!\n");
}

void SockLinkSucessTask::OnCalling() {
	if (m_coped == FALSE) {
		printf("The client %s proxy target %s:%d failed (req: link closed)! \n", m_clientHandler->m_client.name.c_str(),
			inet_ntoa(m_clientHandler->m_client.serverLinkAddr.sin_addr),
			m_clientHandler->m_client.serverLinkAddr.sin_port);
		m_clientHandler->PostProxyFailed();

		//一秒后关闭连接
		AutoRefPtr<SockCloseLinkTask> pTask = new SockCloseLinkTask(GetTickCount(), m_clientHandler);
		m_clientHandler->m_taskDispatchs->PostTask(pTask, 1000);
	}
	m_coped = TRUE;
	m_clientHandler = NULL;
}

SockCloseLinkTask::SockCloseLinkTask(DWORD tm, AutoRefPtr<SocksNetworkTransfered> pClientHandler):
	SockAsyncTask(tm, pClientHandler, SAS_CLOSE_LINK) {

}

SockCloseLinkTask::~SockCloseLinkTask() {
	printf("SockCloseLinkTask destory!\n");
}

void SockCloseLinkTask::OnCalling() {
	if (m_coped == FALSE) {	//任务未被处理
		CancelIo((HANDLE)m_clientHandler->m_socket);
		closesocket(m_clientHandler->m_socket);
	}
	m_coped = TRUE;
	m_clientHandler = NULL;
}

SockKeepAliveTask::SockKeepAliveTask(DWORD tm, AutoRefPtr<SocksNetworkTransfered> pClientHandler, AutoRefPtr<SocksNetworkTransfered> pTargetHandler, BOOL cancel):
	SockAsyncTask(tm, pClientHandler, SAS_KEEPALIVE) {
	m_pTargetHandler = pTargetHandler;
	m_cancel = cancel;
}

SockKeepAliveTask::~SockKeepAliveTask() {
	printf("SockKeepAliveTask destory!\n");
}

void SockKeepAliveTask::OnCalling() {
	if (m_coped == FALSE) {
		if (m_cancel == FALSE) {
			m_clientHandler->m_lockKeepAlives.lock();
			m_clientHandler->m_keepAlives.push_back(m_pTargetHandler);
			m_clientHandler->m_lockKeepAlives.unlock();
		}
		else {
			m_clientHandler->m_lockKeepAlives.lock();
			auto iter = m_clientHandler->m_keepAlives.begin();
			auto iend = m_clientHandler->m_keepAlives.end();
			for (;iter!=iend;iter++){
				if (iter->get() == m_pTargetHandler.get()) {
					m_clientHandler->m_keepAlives.erase(iter);
					break;
				}
			}
			m_clientHandler->m_lockKeepAlives.unlock();
		}
	}
	m_coped = TRUE;
	m_clientHandler = NULL;
	m_pTargetHandler = NULL;
}

SockDomainParseTask::SockDomainParseTask(DWORD tm, AutoRefPtr<SocksNetworkTransfered> pClientHandler, LPCSTR domain, int port):
	SockAsyncTask(tm, pClientHandler, SAS_DOMAIN_PARSER) {
	m_domain = domain;
	m_port = port;
}

SockDomainParseTask::~SockDomainParseTask() {

}

void SockDomainParseTask::OnCalling(void) {
	if (m_coped == FALSE) {
		ADDRINFOEX hints = { 0 };
		ADDRINFOEX *pResult = NULL;
		hints.ai_family = AF_INET;
		hints.ai_flags = AI_PASSIVE;
		hints.ai_socktype = SOCK_STREAM;
		char szPortName[32];
		_itoa_s(m_port, szPortName, 32, 10);
		int status = GetAddrInfoEx(m_domain.c_str(), szPortName,NS_DNS,NULL, &hints, &pResult,NULL,NULL,NULL,NULL);
		if (status == SOCKET_ERROR) {
			m_clientHandler->PostProxyFailed();
			//一秒后关闭连接
			AutoRefPtr<SockCloseLinkTask> pTask = new SockCloseLinkTask(GetTickCount(), m_clientHandler);
			m_clientHandler->m_taskDispatchs->PostTask(pTask, 1000);
		}
		else {
			ADDRINFOEX *pLinkAddr = pResult;
			while (pLinkAddr!=NULL)
			{
				if (pLinkAddr->ai_family == AF_INET) {
					SOCKADDR_IN *pAddr = (SOCKADDR_IN*)pLinkAddr->ai_addr;
					m_clientHandler->m_client.serverLinkAddr.sin_addr.S_un.S_addr = pAddr->sin_addr.S_un.S_addr;
					printf("The client %s proxy to target %s:%d \n", m_clientHandler->m_client.name.c_str(), inet_ntoa(pAddr->sin_addr), htons(pAddr->sin_port));
					AutoRefPtr<SockLinkTargetTask> pTask = new SockLinkTargetTask(GetTickCount(), m_clientHandler);
					m_clientHandler->m_taskDispatchs->PostTask(pTask);
					break;
				}
				pLinkAddr = pLinkAddr->ai_next;
			}
			FreeAddrInfoEx(pResult);
		}
	}
	m_coped = TRUE;
	m_clientHandler = NULL;
}

SockAuthUpdateTask::SockAuthUpdateTask(DWORD tm, AutoRefPtr<SocksNetworkTransfered> pClientHandler):
	SockAsyncTask(tm, pClientHandler, SAS_AUTH_UPDATE) {

}

SockAuthUpdateTask::~SockAuthUpdateTask() {

}

void SockAuthUpdateTask::OnCalling(void) {
	std::list<AutoRefPtr<SocksNetworkTransfered>> dump;

	m_clientHandler->m_lockKeepAlives.lock();
	dump = m_clientHandler->m_keepAlives;
	m_clientHandler->m_lockKeepAlives.unlock();
	for (auto &v : dump)
	{
		int mb = (int)((v->m_dataflow + v->m_pair->m_dataflow) / 1024) / 1024;
		if (m_clientHandler->m_pAuth->Update(v->m_client.user.c_str(), mb) == false) {
			//用户已失效
			CancelIo((HANDLE)v->m_socket);
			closesocket(v->m_socket);
		}
	}
	
	m_clientHandler->m_pAuth->Flush();
}