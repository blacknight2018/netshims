#ifndef IMP_AUTH_MANAGER_H_
#define IMP_AUTH_MANAGER_H_
/**
*      auth_manager.h/auth_manager.cpp
*      ʵ���û���Ȩ����
*/
#include "tinyxml2.h"
#include <map>
#include <string.h>

#ifdef _DEBUG
#define NETSHIMS_DEFAULT		"G:\\C++Studio\\netshims\\build\\Debug\\netconfigure.xml"
#else
#define NETSHIMS_DEFAULT		"netconfigure.xml"
#endif

//�û���Ϣ
struct AUTH_USER_INFO {
	bool enable;
	int maxPower;
	int nowPower;
	__int64 tempstamp;
	int time;
	std::string user;
	std::string pwd;
	tinyxml2::XMLElement *pNode;
};


using namespace tinyxml2;
class AuthManager 
{
public:
	AuthManager(tinyxml2::XMLDocument *pDocument = NULL);
	~AuthManager();

	void operator=(tinyxml2::XMLDocument *pDocument) {
		m_pDocument = pDocument;
	}

public:
	//������Ȩ�û���Ϣ
	void Load(void);

	//ƥ���û�
	bool MatchUser(const char *user,const char *pwd);

	//��ȡʱ���
	__int64 GetTimeStamp(void);

	//�����û�����
	bool Update(const char *user,int flow_mb);

	//��������
	void Flush(void);

private:
	tinyxml2::XMLDocument *m_pDocument;
	std::map<std::string, AUTH_USER_INFO> m_database;
};

#endif
