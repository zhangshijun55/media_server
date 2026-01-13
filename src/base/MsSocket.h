#ifndef MS_SOCKET_H
#define MS_SOCKET_H
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
	virtual ~MsSocket();

	int Bind(const MsInetAddr &addr);
	int Connect(const MsInetAddr &addr);
	int Connect(string &ip, int port);
	int Listen();

	virtual int Accept(shared_ptr<MsSocket> &rSock);
	virtual int Recv(char *buf, int len);
	virtual int Send(const char *buf, int len, int *psend = nullptr);
	virtual int BlockSend(const char *buf, int len);

	int Recvfrom(char *buf, int len, MsInetAddr &addr);
	int Sendto(const char *buf, int len, MsInetAddr &addr);
	void SetNonBlock();
	void SetBlock();
	bool IsTcp();

	MS_SOCKET GetFd();

protected:
	MS_SOCKET m_sock;
};

#endif // MS_SOCKET_H
