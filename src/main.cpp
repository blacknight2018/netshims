#include <stdio.h>
#include "server_dominator.h"
#include "tinyxml2.h"


using namespace tinyxml2;
namespace netconfigure {
	tinyxml2::XMLDocument doc;
}

BOOL OnCmd(const char *option, char *argv[], int bound, tinyxml2::XMLElement *pRoot);

void OnConfigure(void);
void OnSockServer(tinyxml2::XMLElement *pRoot);


int main(int argc,char *argv[]){

	//加载设置
	tinyxml2::XMLError error = netconfigure::doc.LoadFile(NETSHIMS_DEFAULT);
	if (error != tinyxml2::XML_SUCCESS) {
		printf("the file 'netconfigure.xml' load failed! %d \n", error);
		if (error == XML_ERROR_FILE_NOT_FOUND || XML_ERROR_EMPTY_DOCUMENT) {
			OnConfigure();
		}
		return 0;
	}

	////////////////////////////////////////////////////////////////////////////
	if (argc <= 1) {	//帮助
		printf("Usage \n\n");
		printf("netshims [options] <behavior control value> \n\n");
		printf("Welcome to the useage netshims!\n");
		printf("Run 'netshims --help' for more information.\n");
		return 0;
	} 
	else if (argc >= 2 && _strcmpi(argv[1], "-r") == 0) {	//启动服务
	jmp_code:
		printf("Welcome use to netshims! \n");
		printf("- init network .... \n");
		int status = NSServer::DoServerInitialized();
		if (status) {
			printf("error: %d\n", status);
			getchar();
			return status;
		}
		printf("- configure loading ... \n");
		NSServer::Auth = &netconfigure::doc;
		NSServer::Auth.Load();
		printf("======================================== \n");
		OnSockServer(netconfigure::doc.RootElement());
		getchar();
	}
	else {		//cmd 处理
		for (size_t i = 1; i < argc; i++){
			OnCmd(argv[i], &argv[i + 1], argc - i, netconfigure::doc.RootElement());
		}
		netconfigure::doc.SaveFile(NETSHIMS_DEFAULT);
	}
    return 0;
}

BOOL OnCmd(const char *option, char *argv[], int bound, tinyxml2::XMLElement *pRoot) {
	
	if (_strcmpi(option, "--help") == 0) {
		printf("Usage \n\n");
		printf("netshims [options] <behavior control value> \n\n");
		printf("Options\n\n");
		printf("-r				=	Running the service progrm!\n");
		printf("-socks-port <port>		=	Socks-server listen port!\n");
		printf("-socks-enable <true/false>	=	Socks-server is running!\n");
		printf("-socks-auth <true/false>	=	Socks-server is authroized!\n");
		printf("-socks-def-buffer <size>	=	Socks-server default receive buffer!\n");
		printf("-socks-def-accept <count>	=	Socks-server default post accept count!\n");
		printf("-socks-timeout-first <time>	=	Client connect after non sent data(ms)!\n");
		printf("-socks-timeout-link <time>	=	Target server connect timeout(ms)!\n");
		printf("\n");
		printf("-auth-add <user:pwd> [-max-power <data-flow(mb)>] [-time <hour>]\n\n");
		printf("Add authorized users, use option '-max-power' or '-time' to control behavior \n");
		printf("Sample: \nnetshims -user-add admin:1234 -max-power 8000\n");
		printf("netshims -auth-add admin:1234 -time 24\n");
		printf("\n");
		printf("-auth-del <user>		= Cancel this user authroized('#' = all users)\n");
		printf("\n");
		printf("-p <client> [-cancel] = Settings privilege client, provide the IPv4-Address \n");
		printf("-d <client> [-cancel] = Disabled clients, provide the IPv4-Address\n");
		printf("If use '-cancel' options then remove this client\n\n");
	}
	if (option[0]=='-' && bound <= 1) {
	jmp_error:
		if (_strcmpi(option, "-cancel") != 0) {
			printf("option '%s' improper use! \n", option);
		}
		return FALSE;
	}

	if (_strcmpi(option, "-socks-server") == 0) {
		return FALSE;
	} else if (_strcmpi(option, "-socks-port") == 0) {
		XMLNode *pChild = pRoot->FirstChild();
		XMLElement *pElement = NULL;
		while (pChild != NULL)
		{
			pElement = pChild->ToElement();
			if (_strcmpi(pElement->Name(), "socks")) {
				pElement->SetAttribute("port", argv[0]);
				break;
			}
			pChild = pChild->NextSibling();
		}
	}
	else if (_strcmpi(option, "-socks-enable") == 0) {
		XMLNode *pChild = pRoot->FirstChild();
		XMLElement *pElement = NULL;
		while (pChild != NULL)
		{
			pElement = pChild->ToElement();
			if (_strcmpi(pElement->Name(), "socks")) {
				pElement->SetAttribute("enable", argv[0]);
				break;
			}
			pChild = pChild->NextSibling();
		}
	}
	else if (_strcmpi(option, "-socks-auth") == 0) {
		XMLNode *pChild = pRoot->FirstChild();
		XMLElement *pElement = NULL;
		while (pChild != NULL)
		{
			pElement = pChild->ToElement();
			if (_strcmpi(pElement->Name(), "socks")) {
				pElement->SetAttribute("auth", argv[0]);
				break;
			}
			pChild = pChild->NextSibling();
		}
	}
	else if (_strcmpi(option, "-socks-def-buffer") == 0) {
		XMLNode *pChild = pRoot->FirstChild();
		XMLElement *pElement = NULL;
		while (pChild != NULL) {
			pElement = pChild->ToElement();
			if (_strcmpi(pElement->Name(), "socks")) {
				
				pChild = pElement->FirstChild();
				while (pChild){
					pElement = pChild->ToElement();
					if (_strcmpi(pElement->Name(), "SOCKET_DEFAULT_BUFFER") == 0) {
						pElement->SetValue(argv[0]);
						break;
					}
					pChild = pChild->NextSibling();
				}
				break;
			}
			pChild = pChild->NextSibling();
		}
	}
	else if (_strcmpi(option, "-socks-def-accept") == 0) {
		XMLNode *pChild = pRoot->FirstChild();
		XMLElement *pElement = NULL;
		while (pChild != NULL) {
			pElement = pChild->ToElement();
			if (_strcmpi(pElement->Name(), "socks")) {

				pChild = pElement->FirstChild();
				while (pChild) {
					pElement = pChild->ToElement();
					if (_strcmpi(pElement->Name(), "SOCKET_DEFAULT_ACCEPT") == 0) {
						pElement->SetValue(argv[0]);
						break;
					}
					pChild = pChild->NextSibling();
				}
				break;
			}
			pChild = pChild->NextSibling();
		}
	}
	else if (_strcmpi(option, "-socks-timeout-first") == 0) {
		XMLNode *pChild = pRoot->FirstChild();
		XMLElement *pElement = NULL;
		while (pChild != NULL) {
			pElement = pChild->ToElement();
			if (_strcmpi(pElement->Name(), "socks")) {

				pChild = pElement->FirstChild();
				while (pChild) {
					pElement = pChild->ToElement();
					if (_strcmpi(pElement->Name(), "SOCKET_DEFAULT_FIRST_LINK_TIMEOUT") == 0) {
						pElement->SetValue(argv[0]);
						break;
					}
					pChild = pChild->NextSibling();
				}
				break;
			}
			pChild = pChild->NextSibling();
		}
	}
	else if (_strcmpi(option, "-socks-timeout-link") == 0) {
		XMLNode *pChild = pRoot->FirstChild();
		XMLElement *pElement = NULL;
		while (pChild != NULL) {
			pElement = pChild->ToElement();
			if (_strcmpi(pElement->Name(), "socks")) {

				pChild = pElement->FirstChild();
				while (pChild) {
					pElement = pChild->ToElement();
					if (_strcmpi(pElement->Name(), "SOCKET_DEFAULT_LINK_TARGET_TIMEOUT") == 0) {
						pElement->SetValue(argv[0]);
						break;
					}
					pChild = pChild->NextSibling();
				}
				break;
			}
			pChild = pChild->NextSibling();
		}
	}
	else if (_strcmpi(option, "-auth-add") == 0) {
		if (bound >= 2) {

			if (bound == 3 || bound == 5) {
				goto jmp_error;
			}

			char user[32] = "";
			char pwd[32] = "";
			int maxPower = -1;
			int time = -1;
			int index = 1;
			int max = 0;

			if (bound == 4) {
				max = 1;
			}
			else  if (bound == 6) {
				max = 2;
			}

			for (size_t i = 0; i < max; i++)
			{
				if (_strcmpi(argv[index], "-max-power") == 0) {
					maxPower = atoi(argv[index +1]);
				}
				else if (_strcmpi(argv[index], "-time") == 0) {
					time = atoi(argv[index+1]);
				}
				index = index + 2;
			}
			int len = strlen(argv[0]);
			for (size_t i = 0; i < len; i++)
			{
				if (argv[0][i] == ':') {
					memcpy(user, argv[0], i);
					memcpy(pwd, &argv[0][i+1],len-i);
					break;
				}
			}
			if (strlen(user) <= 0 || strlen(pwd) <= 0) {
				goto jmp_error;
			}
			XMLElement *pNewElement;
			XMLText *pNewText;

			XMLNode *pChild = pRoot->FirstChild();
			XMLElement *pElement = NULL;
			while (pChild != NULL) {
				pElement = pChild->ToElement();
				if (_strcmpi(pElement->Name(), "auth") == 0) {
					pNewElement = netconfigure::doc.NewElement("account");
					pNewText = netconfigure::doc.NewText(user);
					pNewElement->InsertEndChild(pNewText);
					pNewElement->SetAttribute("enable", "true");
					pNewElement->SetAttribute("pwd",pwd);
					pNewElement->SetAttribute("max-power",maxPower);
					pNewElement->SetAttribute("now-power", "-1");
					pNewElement->SetAttribute("time", time);
					pNewElement->SetAttribute("timestamp", NSServer::GetTimeStamp());
					pChild->ToElement()->InsertEndChild(pNewElement);
					break;
				}
				pChild = pChild->NextSibling();
			}

		}
	}
	else if (_strcmpi(option, "-auth-del") == 0) {
		XMLNode *pChild = pRoot->FirstChild();
		XMLElement *pElement = NULL;
		while (pChild != NULL) {
			pElement = pChild->ToElement();
			if (_strcmpi(pElement->Name(), "auth") == 0) {
				if (*argv[0] == '#') {
					pElement->DeleteChildren();
					return TRUE;
				}
				pChild = pChild->FirstChild();
				while (pChild!=NULL)
				{
					if (_strcmpi(pChild->ToElement()->GetText(),argv[0]) == 0) {
						pElement->DeleteChild(pChild);
						break;
					}
					pChild = pChild->NextSibling();
				}
				break;
			}
			pChild = pChild->NextSibling();
		}
	}
	else if (_strcmpi(option, "-p") == 0) {
		bool cancel = false;
		if (bound == 3) {
			if (_strcmpi(argv[1], "-cancel") == 0) {
				cancel = true;
			}
		}

		XMLNode *pChild = pRoot->FirstChild();
		XMLElement *pElement = NULL;
		while (pChild != NULL) {
			pElement = pChild->ToElement();
			if (_strcmpi(pElement->Name(), "privilege") == 0) {
				if (cancel == false) {
					XMLNode *pNewElement = netconfigure::doc.NewElement("ip");
					XMLText *pNewText = netconfigure::doc.NewText(argv[0]);
					pNewElement->InsertEndChild(pNewText);
					pElement->InsertEndChild(pNewElement);
				}
				else {
					pChild = pChild->FirstChild();
					while (pChild != NULL) {
						if (_strcmpi(pChild->ToElement()->GetText(), argv[0]) == 0) {
							pElement->DeleteChild(pChild);
							break;
						}
						pChild = pChild->NextSibling();
					}
				}
				break;
			}
			pChild = pChild->NextSibling();
		}
	}
	else if (_strcmpi(option, "-d") == 0) {
		bool cancel = false;
		if (bound == 3) {
			if (_strcmpi(argv[1], "-cancel") == 0) {
				cancel = true;
			}
		}

		XMLNode *pChild = pRoot->FirstChild();
		XMLElement *pElement = NULL;
		while (pChild != NULL) {
			pElement = pChild->ToElement();
			if (_strcmpi(pElement->Name(), "disabled") == 0) {
				if (cancel == false) {
					XMLNode *pNewElement = netconfigure::doc.NewElement("ip");
					XMLText *pNewText = netconfigure::doc.NewText(argv[0]);
					pNewElement->InsertEndChild(pNewText);
					pElement->InsertEndChild(pNewElement);
				}
				else {
					pChild = pChild->FirstChild();
					while (pChild != NULL) {
						if (_strcmpi(pChild->ToElement()->GetText(), argv[0]) == 0) {
							pElement->DeleteChild(pChild);
							break;
						}
						pChild = pChild->NextSibling();
					}
				}
				break;
			}
			pChild = pChild->NextSibling();
		}
	}
	return TRUE;

}

void OnAuthLoad(tinyxml2::XMLElement *pRoot) {

}

void OnSockServer(tinyxml2::XMLElement *pRoot) {
	XMLNode *pChild = pRoot->FirstChild();
	XMLElement *pElement = NULL;

	BOOL enable = FALSE;
	int port = 0;
	BOOL auth = FALSE;
	int defaultBufferSize = SOCKET_DEFAULT_BUFFER;
	int defaultAcceptCount = SOCKET_DEFAULT_ACCEPT;
	int defaultFirstLinkTimeout = SOCKET_DEFAULT_FIRST_LINK_TIMEOUT;
	int defaultLinkTargetTimeout = SOCKET_DEFAULT_LINK_TARGET_TIMEOUT;

	while (pChild != NULL)
	{
		pElement = pChild->ToElement();
		if (_strcmpi(pElement->Name(), "socks") == 0) {
			if (_strcmpi(pElement->Attribute("enable"), "true") == 0) {
				enable = true;
				port = atoi(pElement->Attribute("port"));
				auth = _strcmpi(pElement->Attribute("authorized"), "true") == 0;
				pChild = pChild->FirstChild();
				while (pChild!=NULL)
				{
					if (_strcmpi(pChild->ToElement()->Name(), "SOCKET_DEFAULT_BUFFER") == 0) {
						defaultBufferSize = atoi(pChild->ToElement()->GetText());
					}
					else if (_strcmpi(pChild->ToElement()->Name(), "SOCKET_DEFAULT_ACCEPT") == 0) {
						defaultAcceptCount = atoi(pChild->ToElement()->GetText());
					}
					else if (_strcmpi(pChild->ToElement()->Name(), "SOCKET_DEFAULT_FIRST_LINK_TIMEOUT") == 0) {
						defaultFirstLinkTimeout = atoi(pChild->ToElement()->GetText());
					}
					else if (_strcmpi(pChild->ToElement()->Name(), "SOCKET_DEFAULT_LINK_TARGET_TIMEOUT") == 0) {
						defaultLinkTargetTimeout = atoi(pChild->ToElement()->GetText());
					}
					pChild = pChild->NextSibling();
				}
			}
			break;
		}
		pChild = pChild->NextSibling();
	}

	//启动服务器
	if (enable) {
		NSServer::SocksDominator socks(port, auth,defaultBufferSize,defaultAcceptCount,defaultFirstLinkTimeout,defaultLinkTargetTimeout);
		socks.Run();
	}
}

void OnConfigure(void) {
	tinyxml2::XMLDocument doc;
	XMLElement *pRoot = doc.NewElement("netshims");
	XMLElement *pSocks = doc.NewElement("socks");
	XMLElement *pAuth = doc.NewElement("auth");
	XMLElement *pPrivilege = doc.NewElement("privilege");
	XMLElement *pDisabled = doc.NewElement("disabled");
	XMLElement *pDefBuffer = doc.NewElement("SOCKET_DEFAULT_BUFFER");
	XMLElement *pDefAccept = doc.NewElement("SOCKET_DEFAULT_ACCEPT");
	XMLElement *pFirst = doc.NewElement("SOCKET_DEFAULT_FIRST_LINK_TIMEOUT");
	XMLElement *pLink = doc.NewElement("SOCKET_DEFAULT_LINK_TARGET_TIMEOUT");

	pSocks->SetAttribute("enable", "true");
	pSocks->SetAttribute("port", "1080");
	pSocks->SetAttribute("authorized", "false");

	pDefBuffer->InsertEndChild(doc.NewText("32768"));
	pDefAccept->InsertEndChild(doc.NewText("64"));
	pFirst->InsertEndChild(doc.NewText("3000"));
	pLink->InsertEndChild(doc.NewText("3000"));
	pSocks->InsertEndChild(pDefBuffer);
	pSocks->InsertEndChild(pDefAccept);
	pSocks->InsertEndChild(pFirst);
	pSocks->InsertEndChild(pLink);
	pRoot->InsertEndChild(pSocks);
	pRoot->InsertEndChild(pAuth);
	pRoot->InsertEndChild(pPrivilege);
	pRoot->InsertEndChild(pDisabled);

	doc.InsertEndChild(pRoot);

	doc.SaveFile(NETSHIMS_DEFAULT);
}
