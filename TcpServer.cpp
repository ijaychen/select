/*************************************************************************
	> File Name: TcpServer.cpp
	> Author: cjj
	> Created Time: 2013年10月27日 星期日 01时29分54秒
 ************************************************************************/
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "TcpServer.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>

TcpServer::TcpServer()
{
	pthread_mutex_init(&m_mutex, NULL);
	FD_ZERO(&m_fdReads);
	m_client_socket.clear();
	m_data.clear();
	m_operaFunc = 0;
	m_pidAccept = 0;
	m_pidManage = 0;
}

TcpServer::~TcpServer()
{
	FD_ZERO(&m_fdReads);
	m_client_socket.clear();
	m_data.clear();
	m_operaFunc = NULL;
	pthread_mutex_destroy(&m_mutex);
}

bool TcpServer::Initialize(unsigned int nPort, unsigned long recvFunc)
{
	if(0 != recvFunc)
	{
		//设置回调函数
		m_operaFunc = (pCallBack)recvFunc;
	}
	//先反初始化
	UnInitialize();
	//创建socket
	m_server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(-1 == m_server_socket)
	{
		printf("socket error:%m\n");
		return false;
	}
	//绑定IP和端口
	sockaddr_in serverAddr = {0};
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(nPort);
	serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	int res = bind(m_server_socket, (sockaddr*)&serverAddr, sizeof(serverAddr));
	if(-1 == res)
	{
		printf("bind error:%m\n");
		return false;
	}
	//监听
	res = listen(m_server_socket, MAX_LISTEN);
	if(-1 == res)
	{
		printf("listen error:%m\n");
		return false;
	}
	//创建线程接收socket连接
	if(0 != pthread_create(&m_pidAccept, NULL, AcceptThread, this))
	{
		printf("create accept thread failed\n");
		return false;
	}
	//创建管理线程
	if(0 != pthread_create(&m_pidManage, NULL, ManageThread, this))
	{
		printf("create manage thread failed\n");
		return false;
	}
	return true;
}

//接收socket连接线程
void * TcpServer::AcceptThread(void * pParam)
{
	if(!pParam)
	{
		printf("param is null\n");
		return 0;
	}
	TcpServer * pThis = (TcpServer*)pParam;
	int nMax_fd = 0;
	int i = 0;
	while(1)
	{
		FD_ZERO(&pThis->m_fdReads);
		//把服务器监听的socket添加到监听的文件描述符集合
		FD_SET(pThis->m_server_socket, &pThis->m_fdReads);
		//设置监听的最大文件描述符
		nMax_fd = nMax_fd > pThis->m_server_socket ? nMax_fd : pThis->m_server_socket;
		std::set<int>::iterator iter = pThis->m_client_socket.begin();
		//把客户端对应的socket添加到监听的文件描述符集合
		for(; iter != pThis->m_client_socket.end(); ++iter)
		{
			FD_SET(*iter, &pThis->m_fdReads);
		}
		//判断最大的文件描述符
		if(!pThis->m_client_socket.empty())
		{
			--iter;
			nMax_fd = nMax_fd > (*iter) ? nMax_fd : (*iter);
		}
		//调用select监听所有文件描述符
		int res = select(nMax_fd + 1, &pThis->m_fdReads, 0, 0, NULL);
		if(-1 == res)
		{
			printf("select error:%m\n");
			continue;
		}
		printf("select success\n");
		//判断服务器socket是否可读
		if(FD_ISSET(pThis->m_server_socket, &pThis->m_fdReads))
		{
			//接收新的连接
			int fd = accept(pThis->m_server_socket, 0,0);
			if(-1 == fd)
			{
  	 			printf("accept error:%m\n");
				continue;
			}
			//添加新连接的客户
			pThis->m_client_socket.insert(fd);
			printf("连接成功\n");
		}
		for(iter = pThis->m_client_socket.begin(); iter != pThis->m_client_socket.end(); ++iter)
		{
			//判断客户是否可读
			if(-1 != *iter && FD_ISSET(*iter, &pThis->m_fdReads))
			{
 				unsigned char buf[MAX_DATA_LEN] = {0};
				res = recv(*iter, buf, sizeof(buf), 0);
				if(res > 0)
				{
					ServerData serverData = {0};
					memcpy(serverData.buf, buf, res);
					serverData.nLen = res;
					serverData.socket = *iter;
					pthread_mutex_lock(&pThis->m_mutex);
					pThis->m_data.push_back(serverData);
					pthread_mutex_unlock(&pThis->m_mutex);
				}
				else if(0 == res)
				{
					printf("客户端退出\n");
					pThis->m_client_socket.erase(iter);
				}
				else
				{
					printf("recv error\n");
				}	
			}
		}
	}
}

//发送数据到指定的socket
bool TcpServer::SendData(const unsigned char * buf, size_t len, int sock)
{
	if(NULL == buf)
	{
		return false;
	}
	int res = send(sock, buf, len, 0);
	if(-1 == res)
	{
		printf("send error:%m\n");
		return false;
	}
	return true;
}

//管理线程，用于创建处理线程
void * TcpServer::ManageThread(void * pParam)
{
	if(!pParam)
	{
		return 0;
	}
	pthread_t pid;
	TcpServer * pThis = (TcpServer *)pParam;
	while(1)
	{
		//使用互斥量
		pthread_mutex_lock(&pThis->m_mutex);	
		int nCount = pThis->m_data.size();
		pthread_mutex_unlock(&pThis->m_mutex);
		if(nCount > 0)
		{
			pid = 0;
			//创建处理线程
			if( 0 != pthread_create(&pid, NULL, OperatorThread, pParam))
			{
				printf("creae operator thread failed\n");
			}
		}
		//防止抢占CPU资源
		usleep(100);
	}
}

//数据处理线程
void * TcpServer::OperatorThread(void * pParam)
{
	if(!pParam)
	{
		return 0;
	}
	TcpServer * pThis = (TcpServer*)pParam;
	pthread_mutex_lock(&pThis->m_mutex);
	if(!pThis->m_data.empty())
	{
		ServerData data = pThis->m_data.front();
		pThis->m_data.pop_front();
		if(pThis->m_operaFunc)
		{
			//把数据交给回调函数处理
			pThis->m_operaFunc((char *)&data, sizeof(data), data.socket);
		}
	}
	pthread_mutex_unlock(&pThis->m_mutex);
}

bool TcpServer::UnInitialize()
{
	close(m_server_socket);
	for(std::set<int>::iterator iter = m_client_socket.begin(); iter != m_client_socket.end(); ++iter)
	{
		close(*iter);
	}
	m_client_socket.clear();
	if(0 != m_pidAccept)
	{
		pthread_cancel(m_pidAccept);
	}
	if(0 != m_pidManage)
	{
		pthread_cancel(m_pidManage);
	}
}
