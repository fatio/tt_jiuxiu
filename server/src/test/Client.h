/*================================================================
*     Copyright (c) 2015年 lanhu. All rights reserved.
*   
*   文件名称：Client.h
*   创 建 者：Zhang Yuanhao
*   邮    箱：bluefoxah@gmail.com
*   创建日期：2015年01月20日
*   描    述：
*
#pragma once
================================================================*/
#ifndef __CLIENT_H__
#define __CLIENT_H__
#include <iostream>
#include "ostype.h"
#include "IM.BaseDefine.pb.h"
#include "IPacketCallback.h"
#include "../base/EventDispatch.h"
#include <map>
#include <memory>
using namespace std;

class CClient:public IPacketCallback
{
public:
    CClient(uint32_t user_id, uint32_t app_id, uint32_t domain_id);
    ~CClient();
public:
    uint32_t getUserId(){ return m_userId; }
    uint32_t getAppId() { return m_appId; }
    uint32_t getDomainId() { return m_domainId; }
    static void TimerCallback(void* callback_data, uint8_t msg, uint32_t handle, void* pParam);
    
public:
    void connect();
    void close();
    uint32_t login();
    uint32_t getUserInfo(list<uint32_t>& lsUserId);
    uint32_t sendMsg(uint32_t nToId,IM::BaseDefine::MsgType nType, const string& strMsg);
    uint32_t getUnreadMsgCnt();
    uint32_t getRecentSession();
    uint32_t getMsgList(IM::BaseDefine::SessionType nType, uint32_t nPeerId, uint32_t nMsgId, uint32_t nMsgCnt);
    uint32_t sendReadAck();
public:
    virtual void onError(uint32_t nSeqNo, uint32_t nCmd, const string& strMsg);
    virtual void onConnect();
    virtual void onClose();
    virtual void onLogin(uint32_t nSeqNo, uint32_t nResultCode, string& strMsg, IM::BaseDefine::UserInfo* pUser = NULL);
    virtual void onGetUserInfo(uint32_t nSeqNo, const list<IM::BaseDefine::UserInfo>& lsUser);
    virtual void onSendMsg(uint32_t nSeqNo, uint32_t nSendId, uint32_t nRecvId, IM::BaseDefine::SessionType nType, uint32_t nMsgId);
    virtual void onGetUnreadMsgCnt(uint32_t nSeqNo, uint32_t nUserId, uint32_t nTotalCnt, const list<IM::BaseDefine::UnreadInfo>& lsUnreadCnt);
    virtual void onGetRecentSession(uint32_t nSeqNo, uint32_t nUserId, const list<IM::BaseDefine::ContactSessionInfo>& lsSession);
    virtual void onGetMsgList(uint32_t nSeqNo, uint32_t nUserId, uint32_t nPeerId, IM::BaseDefine::SessionType nType, uint32_t nMsgId, uint32_t nMsgCnt, const list<IM::BaseDefine::MsgInfo>& lsMsg);
    virtual void onRecvMsg(uint32_t nSeqNo, uint32_t nFromId, uint32_t nToId, uint32_t nMsgId, uint32_t nCreateTime, IM::BaseDefine::MsgType nMsgType, const string& strMsgData);
    virtual bool     isLogined() { return m_bLogined; }
private:
    uint32_t        m_userId;
    uint32_t        m_appId;
    uint32_t        m_domainId;
    string          m_token;
    uint32_t        m_nLastGetUser;
    uint32_t        m_nLastGetSession;
    net_handle_t    m_nHandle;
    string 			  m_szIP;
    string 		     m_szPort;
    bool	    m_bLogined;
    IM::BaseDefine::UserInfo m_cSelfInfo;
    rb_timer_item m_timer;
};


class CClientManager : public std::enable_shared_from_this<CClientManager>
{
public:
	CClientManager();
	virtual ~CClientManager();

	int AddClientConn(int nUserId, CClient* pClientObj);

	void DestoryClientConn(int nUserId);

private:
	std::map<int,CClient*> m_mapClientConn;
	typedef std::map<int,CClient*>::iterator ITER_MAP_CLIENT_OBJ;
	int m_nClients;   //the number of current management of the client

};

typedef std::shared_ptr<CClientManager> client_manager_ptr;

#endif /*defined(__CLIENT_H__) */
