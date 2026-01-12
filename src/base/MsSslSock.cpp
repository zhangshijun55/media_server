#include "MsSslSock.h"
#include "MsLog.h"

int MsSslSock::Accept(shared_ptr<MsSocket> &rSock) {
	struct sockaddr_in inAddr;
	socklen_t addrLen = sizeof(inAddr);

	MS_SOCKET s = accept(m_sock, (struct sockaddr *)&inAddr, &addrLen);

	if (s == MS_INVALID_SOCKET) {
		return -1;
	}

	SSL *ssl = SSL_new(m_sslCtx);
	SSL_set_fd(ssl, s);
	if (SSL_accept(ssl) <= 0) {
		ERR_print_errors_fp(stderr);
		SSL_free(ssl);
		close(s);
		return -1;
	}

	rSock = make_shared<MsSslSock>(s, m_sslCtx, ssl);

	return 0;
}

int MsSslSock::Recv(char *buf, int len) { return SSL_read(m_ssl, buf, len); }

int MsSslSock::Send(const char *buf, int len, int *psend) {
	int ret = 0;
	int snd = 0;

	while (len > 0) {
		ret = SSL_write(m_ssl, buf, len);
		if (ret < 0) {
			int err = SSL_get_error(m_ssl, ret);
			if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
				ret = MS_TRY_AGAIN;
				break;
			} else if (MS_LAST_ERROR == EINTR) {
				continue;
			} else {
				MS_LOG_ERROR("ssl send error,err:%d %d", err, MS_LAST_ERROR);
				break;
			}
		}

		if (ret > 0) {
			buf += ret;
			len -= ret;
			snd += ret;
		}
	}

	if (psend) {
		*psend = snd;
	}

	return ret < 0 ? ret : snd;
}

int MsSslSock::BlockSend(const char *buf, int len) {
	int ret = 0;
	const char *pBuf = buf;
	int pLen = len;

	while (pLen > 0) {
		ret = SSL_write(m_ssl, pBuf, pLen);
		if (ret >= 0) {
			pLen -= ret;
			pBuf += ret;
		} else {
			int err = SSL_get_error(m_ssl, ret);
			if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
				continue;
			} else if (MS_LAST_ERROR == EINTR) {
				continue;
			} else {
				MS_LOG_ERROR("send error,err:%d", MS_LAST_ERROR);
				return -1;
			}
		}
	}

	return 0;
}