#ifndef MS_SSL_SOCK_H
#define MS_SSL_SOCK_H
#include "MsSocket.h"

#include <openssl/err.h>
#include <openssl/ssl.h>

class MsSslSock : public MsSocket {
public:
	MsSslSock(int af, int type, int protocol, SSL_CTX *ctx)
	    : MsSocket(af, type, protocol), m_sslCtx(ctx), m_ssl(nullptr) {};

	MsSslSock(MS_SOCKET s, SSL_CTX *ctx, SSL *ssl) : MsSocket(s), m_sslCtx(ctx), m_ssl(ssl) {}

	~MsSslSock() {
		if (m_ssl) {
			SSL_shutdown(m_ssl);
			SSL_free(m_ssl);
			m_ssl = nullptr;
		}
	}

	int Accept(shared_ptr<MsSocket> &rSock) override;
	int Recv(char *buf, int len) override;
	int Send(const char *buf, int len, int *psend = nullptr) override;
	int BlockSend(const char *buf, int len) override;

private:
	SSL *m_ssl;
	SSL_CTX *m_sslCtx;
};

#endif // MS_SSL_SOCK_H