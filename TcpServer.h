/*************************************************************************
	> File Name: TcpServer.h
	> Author: cjj
	> Created Time: 2013年10月27日 星期日 01时01分23秒
 ************************************************************************/
#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include "command_define.h"
#include <set>
#include <list>
#include <sys/select.h>
#include <pthread.h>

class TcpServer
{
public:
	TcpServer();
	virtual ~TcpServer();
	bool Initialize(unsigned int nPort, unsigned long recvFunc);
	bool SendData(const unsigned char * szBuf, size_t nLen, int socket);
	bool UnInitialize();

private:
	static void * AcceptThread(void * pParam);
	static void * OperatorThread(void * pParam);	
	static void * ManageThread(void * pParam);
private:
	int m_server_socket;
	fd_set m_fdReads;
	pthread_mutex_t m_mutex;
	pCallBack m_operaFunc;
	//int m_client_socket[MAX_LISTEN];
	std::set<int> m_client_socket;
	std::list<ServerData> m_data;
	pthread_t m_pidAccept;
	pthread_t m_pidManage;
};

#endif
