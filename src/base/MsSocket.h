#pragma once
#include "MsInetAddr.h"
#include "MsOsConfig.h"
#include <memory>

enum {
	MS_TRY_AGAIN = -1000,
};

class MsSocket {
public:
	MsSocket(MS_SOCKET s);
	MsSocket(int af, int type, int protocol);
	~MsSocket();

	int Bind(const MsInetAddr &addr);
	int Connect(const MsInetAddr &addr);
	int Connect(string &ip, int port);
	int Listen();
	int Accept(shared_ptr<MsSocket> &rSock);
	int Recv(char *buf, int len);
	int Recvfrom(char *buf, int len, MsInetAddr &addr);
	int Send(const char *buf, int len, int *psend = nullptr);
	int BlockSend(const char *buf, int len);
	int Sendto(const char *buf, int len, MsInetAddr &addr);
	void SetNonBlock();
	void SetBlock();
	bool IsTcp();

	MS_SOCKET GetFd();

private:
	MS_SOCKET m_sock;
};
