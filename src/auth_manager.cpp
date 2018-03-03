#include "auth_manager.h"
#include <time.h>
#include <ctime>
#include <chrono>

using timestamp = unsigned long long;

AuthManager::AuthManager(tinyxml2::XMLDocument *pDocument):m_pDocument(pDocument) {

}

AuthManager::~AuthManager() {

}

void AuthManager::Load(void) {
	if (m_pDocument != NULL) {
		tinyxml2::XMLElement *pAuth = NULL;
		tinyxml2::XMLNode *pChild = m_pDocument->RootElement()->FirstChild();
		tinyxml2::XMLElement *pAccount = NULL;
		AUTH_USER_INFO info = { 0 };
		while (pChild!=NULL)
		{
			if (_strcmpi(pChild->ToElement()->Name(), "auth") == 0) {
				pAuth = pChild->ToElement();
				pChild = pAuth->FirstChild();
				while (pChild!=NULL)
				{
					pAccount = pChild->ToElement();
					if (_strcmpi(pAccount->Attribute("enable"), "true") == 0) {
						info.enable = true;
						info.user = pAccount->GetText();
						info.pwd = pAccount->Attribute("pwd");
						info.maxPower = atoi(pAccount->Attribute("max-power"));
						info.nowPower = atoi(pAccount->Attribute("now-power"));
						info.time = atoi(pAccount->Attribute("time"));
						info.tempstamp = atoll(pAccount->Attribute("timestamp"));
						m_database.insert(std::make_pair(info.user,info));
					}
					pChild = pChild->NextSibling();
				}
				break;
			}
			pChild = pChild->NextSibling();
		}
	}
}

//匹配用户
bool AuthManager::MatchUser(const char *user, const char *pwd) {
	for (auto &v:m_database)
	{
		if (v.first == user && v.second.pwd == pwd) {
			if (v.second.enable) {
				if (v.second.maxPower == -1 || v.second.time == -1) {
					//没有启用流量以及时长限制
					return true;
				}
				else if (v.second.maxPower != -1) {
					//启用了流量限制
					if (v.second.nowPower < v.second.maxPower) {
						return true;
					}
				}
				else  if (v.second.time != -1) {
					// 启用了时长限制
					timestamp tmp = (timestamp)GetTimeStamp();
					__int64 tm = (__int64)(tmp - (timestamp)v.second.tempstamp);
					__int64 hour = (((tm / 1000) / 60) / 60); //ms -> s -> m -> h
					if (hour < v.second.time) {
						return true;
					}
				}
			}
		}
	}
	return false;
}

//获取时间戳
__int64 AuthManager::GetTimeStamp(void) {
	std::chrono::time_point<std::chrono::system_clock,
		std::chrono::milliseconds> tp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
	auto tmp = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch());
	return  (__int64)tmp.count();
}

//更新用户数据
bool AuthManager::Update(const char *user, int flow_mb) {
	auto &iter = m_database.find(user);
	if (iter != m_database.end()) {
		if (iter->second.enable == false) {
			return false;
		}
		if (iter->second.maxPower != -1) {
			iter->second.nowPower += flow_mb;
			iter->second.pNode->SetAttribute("now-power", iter->second.nowPower);
			if (iter->second.nowPower >= iter->second.maxPower) {
				//流量达到上限
				iter->second.enable = false;
				iter->second.pNode->SetAttribute("enable", "false");
				return false;
			}
		}
		if (iter->second.time != -1) {
			timestamp tmp = (timestamp)GetTimeStamp();
			__int64 tm = (__int64)(tmp - (timestamp)iter->second.tempstamp);
			__int64 hour = (((tm / 1000) / 60) / 60); //ms -> s -> m -> h
			if (hour >= iter->second.time) {
				//时长达到上限
				iter->second.enable = false;
				iter->second.pNode->SetAttribute("enable", "false");
				return false;
			}
		}
		return true;
	}
	return false;
}

//保存数据
void AuthManager::Flush(void) {
	m_pDocument->SaveFile(NETSHIMS_DEFAULT);
}