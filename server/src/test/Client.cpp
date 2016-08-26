/*================================================================
*     Copyright (c) 2015年 lanhu. All rights reserved.
*   
*   文件名称：Client.cpp
*   创 建 者：Zhang Yuanhao
*   邮    箱：bluefoxah@gmail.com
*   创建日期：2015年01月20日
*   描    述：
*
================================================================*/
#include "Client.h"
#include "HttpClient.h"
#include "Common.h"
#include "json/json.h"
#include "ClientConn.h"
#include "ConfigFileReader.h"


static ClientConn*  g_pConn = NULL;

CClient::CClient(uint32_t user_id, uint32_t app_id, uint32_t domain_id):
m_userId(user_id),
m_appId(app_id),
m_domainId(domain_id),
m_nLastGetUser(0),
m_bLogined(false)
{
	rbtimer_init(&m_timer,CClient::TimerCallback,NULL,1000,0,0);
}

CClient::~CClient()
{
}

void CClient::TimerCallback(void* callback_data, uint8_t msg, uint32_t handle, void* pParam)
{
    if (g_pConn != NULL && g_pConn->isLogined() == true) {
        uint64_t cur_time = get_tick_count();
        g_pConn->OnTimer(cur_time);
    }
}



void CClient::onError(uint32_t nSeqNo, uint32_t nCmd,const string& strMsg)
{
    g_imlog.Error("get error:%d, msg:%s", nCmd, strMsg.c_str());
}

void CClient::connect()
{
   CConfigFileReader config_file("test_client.conf");
   m_szIP = config_file.GetConfigName("LoginServerIP");
   m_szPort = config_file.GetConfigName("LoginServerPort");
   CHttpClient httpClient;
    string user_params = "user_id=" + int2string(m_userId) + "&app_id=" + int2string(m_appId) + "&domain_id=" + int2string(m_domainId);
    //string unregister_url = "http://192.168.100.64:8081?method_id=im.unregister&" + user_params;
    string unregister_url = "http://" + m_szIP + ":" + m_szPort +  "?method_id=im.unregister&" + user_params;
    //string connect_url = "http://192.168.100.64:8081?method_id=im.connect&" + user_params;
    string connect_url = "http://" + m_szIP + ":" + m_szPort + "?method_id=im.connect&" + user_params;
    string strResp;

    CURLcode nRet = httpClient.Get(unregister_url, strResp);
    if(nRet != CURLE_OK) {
        printf("unregister falied. access url:%s error\n", unregister_url.c_str());
        PROMPTION;
        return;
    }
    Json::Reader reader;
    Json::Value value;
    if(!reader.parse(strResp, value)) {
        printf("unregister falied. parse response error:%s\n", strResp.c_str());
        PROMPTION;
        return;
    }

    try {
    	uint32_t result_code = value["result_code"].asUInt();
	if (result_code != 0) {
		printf("unregister failed. code:%u\n", result_code);
		PROMPTION;
		return;
	}
    } catch (std::runtime_error msg) {
        printf("unregister failed. get json error:%s\n", strResp.c_str());
	PROMPTION;
	return;
    }

    strResp.clear();
    nRet = httpClient.Get(connect_url, strResp);
    if (nRet != CURLE_OK) {
        printf("connect failed, access url:%s error\n", connect_url.c_str());
	PROMPTION;
        return;
    }

    printf("connect response: %s\n", strResp.c_str());
    if (!reader.parse(strResp, value)) {
        printf("connect failed, parse response error:%s\n", strResp.c_str());
	PROMPTION;
	return;
    }
    
    string strPriorIp, strBackupIp;
    uint16_t nPort;
    try {
        uint32_t result_code = value["result_code"].asUInt();
        if(result_code != 0) {
            printf("connect falied. code:%u\n", result_code);
            PROMPTION;
            return;
        }
        strPriorIp = value["prior_ip"].asString();
        strBackupIp = value["backup_ip"].asString();
        nPort = value["port"].asUInt();
	m_token = value["user_token"].asString();
	printf("ip:%s, port:%d, token:%s\n", strPriorIp.c_str(), nPort, m_token.c_str());
    } catch (std::runtime_error msg) {
        printf("connect falied. get json error:%s\n", strResp.c_str());
        PROMPTION;
        return;
    }
    
    g_pConn = new ClientConn(this);
    m_nHandle = g_pConn->connect(strPriorIp.c_str(), nPort, m_userId, m_appId, m_domainId, m_token);
    if(m_nHandle != INVALID_SOCKET) {
        //netlib_register_timer(CClient::TimerCallback, NULL, 1000);
    	    netlib_register_timer(&m_timer);
    } else {
        printf("invalid socket handle\n");
    }
}

void CClient::onConnect()
{
    login();
}


void CClient::close()
{
    g_pConn->Close();
}

void CClient::onClose()
{
    
}

uint32_t CClient::login()
{
    return g_pConn->login(m_userId, m_appId, m_domainId, m_token);
}

void CClient::onLogin(uint32_t nSeqNo, uint32_t nResultCode, string& strMsg, IM::BaseDefine::UserInfo* pUser)
{
    if(nResultCode != 0)
    {
        printf("login failed.errorCode=%u, msg=%s\n",nResultCode, strMsg.c_str());
        return;
    }
    if(pUser)
    {
        m_cSelfInfo = *pUser;
        m_bLogined = true;
	printf("login success. msg=%s\n", strMsg.c_str());
    }
    else
    {
        printf("pUser is null\n");
    }
}

uint32_t CClient::getUserInfo(list<uint32_t>& lsUserId)
{
    uint32_t nUserId = m_cSelfInfo.user_id();
    return g_pConn->getUserInfo(nUserId, lsUserId);
}

void CClient::onGetUserInfo(uint32_t nSeqNo, const list<IM::BaseDefine::UserInfo> &lsUser)
{
    
}

uint32_t CClient::sendMsg(uint32_t nToId, IM::BaseDefine::MsgType nType, const string &strMsg)
{
    uint32_t nFromId = m_cSelfInfo.user_id();
    return g_pConn->sendMessage(nFromId, nToId, nType, strMsg);
}

void CClient::onSendMsg(uint32_t nSeqNo, uint32_t nSendId, uint32_t nRecvId, IM::BaseDefine::SessionType nType, uint32_t nMsgId)
{
    printf("send msg succes. From:%u, To:%u, seqNo:%u, msgId:%u\n", nSendId, nRecvId, nSeqNo, nMsgId);
}


uint32_t CClient::getUnreadMsgCnt()
{
    uint32_t nUserId = m_cSelfInfo.user_id();
    return g_pConn->getUnreadMsgCnt(nUserId);
}

void CClient::onGetUnreadMsgCnt(uint32_t nSeqNo, uint32_t nUserId, uint32_t nTotalCnt, const list<IM::BaseDefine::UnreadInfo>& lsUnreadCnt)
{
    printf("User:%u, have %u unread message\n", nUserId, nTotalCnt); 
}

uint32_t CClient::getRecentSession()
{
    uint32_t nUserId = m_cSelfInfo.user_id();
    return g_pConn->getRecentSession(nUserId, m_nLastGetSession);
}

void CClient::onGetRecentSession(uint32_t nSeqNo, uint32_t nUserId, const list<IM::BaseDefine::ContactSessionInfo> &lsSession)
{
    
}

uint32_t CClient::getMsgList(IM::BaseDefine::SessionType nType, uint32_t nPeerId, uint32_t nMsgId, uint32_t nMsgCnt)
{
    uint32_t nUserId = m_cSelfInfo.user_id();
    return g_pConn->getMsgList(nUserId, nType, nPeerId, nMsgId, nMsgCnt);
}

void CClient::onGetMsgList(uint32_t nSeqNo, uint32_t nUserId, uint32_t nPeerId, IM::BaseDefine::SessionType nType, uint32_t nMsgId, uint32_t nMsgCnt, const list<IM::BaseDefine::MsgInfo> &lsMsg)
{
    
}

void CClient::onRecvMsg(uint32_t nSeqNo, uint32_t nFromId, uint32_t nToId, uint32_t nMsgId, uint32_t nCreateTime, IM::BaseDefine::MsgType nMsgType, const string &strMsgData)
{
    printf("Dear %u, you have a message from:%u \"%s\"\n", nToId, nFromId, strMsgData.c_str());
}


CClientManager::CClientManager():m_nClients(0)
{

}


CClientManager::~CClientManager()
{
}


int CClientManager::AddClientConn(int nUserId, CClient* pClientObj)
{
	//ITER_MAP_CLIENT_OBJ pIterFound;
	//std::pair<std::map<int,CClient*>::iterator,bool> pIterInsert =
	std::pair<ITER_MAP_CLIENT_OBJ,bool> pIterInsert =
			m_mapClientConn.insert(std::make_pair(nUserId,pClientObj));

	if (pIterInsert.second)
	{
		m_nClients++;
		return 0;
	}

	return -1;
}


void CClientManager::DestoryClientConn(int nUserId)
{

}
