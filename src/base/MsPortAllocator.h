#ifndef MS_PORT_ALLOCATOR_H
#define MS_PORT_ALLOCATOR_H

#include "MsSocket.h"
#include <condition_variable>
#include <mutex>

#if ENABLE_HTTPS
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

class MsPortAllocator {
public:
	MsPortAllocator();

	shared_ptr<MsSocket> AllocPort(int type, string &ip, int &port);

	static MsPortAllocator *Instance();

#if ENABLE_HTTPS
	SSL_CTX *GetSslCtx();
#endif

private:
	int m_minPort;
	int m_maxPort;
	int m_curPort;

	static unique_ptr<MsPortAllocator> m_instance;
	static mutex m_mutex;
	static condition_variable m_condiVar;
};

#endif // MS_PORT_ALLOCATOR_H
