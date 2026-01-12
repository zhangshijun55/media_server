#include "MsGbServerHandler.h"
#include "MsLog.h"
#include "MsMsgDef.h"
#include <string.h>

MsGbServerHandler::MsGbServerHandler(const shared_ptr<MsIGbServer> &server)
    : m_server(server), m_bufSize(DEF_BUF_SIZE), m_bufOff(0) {
	m_buf = (char *)malloc(m_bufSize);
}

MsGbServerHandler::~MsGbServerHandler() { free(m_buf); }

void MsGbServerHandler::HandleRead(shared_ptr<MsEvent> evt) {
	shared_ptr<MsSocket> sock = evt->GetSharedSocket();
	int recv = 0;
	bool isTcp = sock->IsTcp();

	if (isTcp) {
		recv = sock->Recv(m_buf + m_bufOff, m_bufSize - 1 - m_bufOff);
	} else {
		recv = sock->Recvfrom(m_buf + m_bufOff, m_bufSize - 1 - m_bufOff, m_recvAddr);
	}

	if (recv <= 0) {
		return;
	}

	m_bufOff += recv;
	m_buf[m_bufOff] = '\0';
	int oriTotal = m_bufOff;
	char *p2 = m_buf;

	while (m_bufOff > 0) {
		if (!IsHeaderComplete(p2)) {
			if (m_bufOff == m_bufSize - 1) // header too large, reset buf
			{
				MS_LOG_DEBUG("gb server buf full: %s", m_buf);
				m_bufOff = 0;
			} else if (m_bufOff) {
				if (p2 != m_buf) {
					MS_LOG_DEBUG("gb server left:%d msg:%s", m_bufOff, p2);

					if (CheckVaildHeader(p2)) {
						memmove(m_buf, p2, m_bufOff);
					} else {
						MS_LOG_DEBUG("gb server unknown msg:%s", p2);
						m_bufOff = 0;
						return;
					}
				}
			}
			return;
		}

		if (!CheckVaildHeader(p2)) {
			MS_LOG_DEBUG("gb server invalid msg:%s", p2);
			m_bufOff = 0;
			return;
		}

		char *oriP2 = p2;
		MsSipMsg sipMsg;
		sipMsg.Parse(p2);
		int cntLen = sipMsg.m_contentLength.GetIntVal();
		int left = m_bufOff - (p2 - oriP2);

		if (left < cntLen) {
			char *op2 = p2;
			p2 = oriP2;
			if (m_bufOff) {
				MS_LOG_DEBUG("gb server bufoff:%d msg header:\n%s", m_bufOff,
				             string(oriP2, op2 - oriP2).c_str());
				MS_LOG_DEBUG("body:\n%s", op2);
				MS_LOG_DEBUG("gb server need len:%d left:%d", cntLen, left);
				memmove(m_buf, p2, m_bufOff);
			}
			return;
		}

		MS_LOG_VERBS("recv cnt len:%d msg header:\n%s", cntLen, string(oriP2, p2 - oriP2).c_str());
		if (cntLen > 0) {
			MS_LOG_VERBS("body:\n%s", string(p2, cntLen).c_str());
		}

		if (sipMsg.m_vias.back().HasRport() && !isTcp) {
			sipMsg.m_vias.back().Rebuild(sipMsg.m_vias.back().GetTransport(),
			                             sipMsg.m_vias.back().GetBranch(), m_recvAddr);
		}

		if (sipMsg.m_status.size()) {
			m_server->HandleResponse(sipMsg, sock, p2, cntLen);
		} else if (sipMsg.m_method == "REGISTER") {
			m_server->HandleRegist(sipMsg, sock, m_recvAddr);
		} else if (sipMsg.m_method == "MESSAGE") {
			m_server->HandleMessage(sipMsg, sock, m_recvAddr, p2, cntLen);
		} else if (sipMsg.m_method == "INVITE") {
			m_server->HandleInvite(sipMsg, sock, m_recvAddr, p2, cntLen);
		} else if (sipMsg.m_method == "BYE") {
			m_server->HandleBye(sipMsg, sock, m_recvAddr);
		} else if (sipMsg.m_method == "ACK") {
			m_server->HandleAck(sipMsg);
		} else if (sipMsg.m_method == "NOTIFY") {
			m_server->HandleNotify(sipMsg, sock, m_recvAddr, p2, cntLen);
		} else if (sipMsg.m_method == "CANCEL") {
			m_server->HandleCancel(sipMsg, sock, m_recvAddr);
		} else {
			MS_LOG_WARN("unknown:%s", sipMsg.m_method.c_str());
		}

		p2 += cntLen;
		// m_bufOff -= (p2 - oriP2);
		int consumed = p2 - m_buf;
		m_bufOff = oriTotal - consumed;
		MS_LOG_DEBUG("gb server buff consumed:%d left:%d", consumed, m_bufOff);
	}
}

void MsGbServerHandler::HandleClose(shared_ptr<MsEvent> evt) {
	MS_LOG_ERROR("gb server handler close");

	m_server->DelEvent(evt);

	MsMsg msg;
	msg.m_msgID = MS_GB_SERVER_HANDLER_CLOSE;
	msg.m_dstType = m_server->GetType();
	msg.m_dstID = m_server->GetID();
	msg.m_intVal = evt->GetSocket()->GetFd();
	m_server->EnqueMsg(msg);
}

bool MsGbServerHandler::CheckVaildHeader(char *p2) {
	return strncmp(p2, "MESSAGE", strlen("MESSAGE")) == 0 ||
	       strncmp(p2, "REGISTER", strlen("REGISTER")) == 0 ||
	       strncmp(p2, "INVITE", strlen("INVITE")) == 0 || strncmp(p2, "BYE", strlen("BYE")) == 0 ||
	       strncmp(p2, "ACK", strlen("ACK")) == 0 || strncmp(p2, "NOTIFY", strlen("NOTIFY")) == 0 ||
	       strncmp(p2, "CANCEL", strlen("CANCEL")) == 0 ||
	       strncmp(p2, "SIP/2.0", strlen("SIP/2.0")) == 0;
}

void MsGbAcceptHandler::HandleRead(shared_ptr<MsEvent> evt) {
	shared_ptr<MsSocket> sock = evt->GetSharedSocket();
	shared_ptr<MsSocket> clientSock;
	int ret = sock->Accept(clientSock);
	if (ret < 0) {
		MS_LOG_ERROR("gb server accept err:%d", ret);
		return;
	}

	shared_ptr<MsEventHandler> h = make_shared<MsGbServerHandler>(m_server);
	shared_ptr<MsEvent> clientEvent = make_shared<MsEvent>(clientSock, MS_FD_READ | MS_FD_CLOSE, h);
	m_server->AddEvent(clientEvent);
}

void MsGbAcceptHandler::HandleClose(shared_ptr<MsEvent> evt) { m_server->DelEvent(evt); }
