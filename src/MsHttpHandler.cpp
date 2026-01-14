#include "MsHttpHandler.h"
#include "MsCommon.h"
#include "MsEvent.h"
#include "MsHttpServer.h"
#include "MsLog.h"
#include "nlohmann/json.hpp"
#include <map>
#include <string.h>

using json = nlohmann::json;

MsHttpAcceptHandler::MsHttpAcceptHandler(const shared_ptr<MsIHttpServer> &server)
    : m_server(server) {}

void MsHttpAcceptHandler::HandleRead(shared_ptr<MsEvent> evt) {
	MsSocket *sock = evt->GetSocket();
	shared_ptr<MsSocket> s;

	if (sock->Accept(s)) {
		MS_LOG_WARN("accept err:%d", MS_LAST_ERROR);
		return;
	}

	shared_ptr<MsEventHandler> evtHandler = make_shared<MsHttpHandler>(m_server);
	shared_ptr<MsEvent> msEvent = make_shared<MsEvent>(s, MS_FD_READ | MS_FD_CLOSE, evtHandler);
	m_server->AddEvent(msEvent);
}

void MsHttpAcceptHandler::HandleClose(shared_ptr<MsEvent> evt) { m_server->DelEvent(evt); }

MsHttpHandler::MsHttpHandler(const shared_ptr<MsIHttpServer> &server)
    : m_server(server), m_bufSize(DEF_BUF_SIZE), m_bufOff(0) {
	m_bufPtr = make_unique<char[]>(m_bufSize);
}

MsHttpHandler::~MsHttpHandler() {}

void MsHttpHandler::HandleRead(shared_ptr<MsEvent> evt) {
	MsSocket *sock = evt->GetSocket();

	int recv = sock->Recv(m_bufPtr.get() + m_bufOff, m_bufSize - 1 - m_bufOff);

	if (recv <= 0) {
		return;
	}

	m_bufOff += recv;
	m_bufPtr[m_bufOff] = '\0';

	char *p2 = m_bufPtr.get();

	while (m_bufOff) {
		if (!IsHeaderComplete(p2)) {
			if (m_bufOff) {
				if (m_bufOff > 16384) {
					MS_LOG_ERROR("left buf over 16k, reset");
					m_bufOff = 0;
				} else if (p2 != m_bufPtr.get()) {
					MS_LOG_DEBUG("http buf left:%d", m_bufOff);
					memmove(m_bufPtr.get(), p2, m_bufOff);
				}
			}

			return;
		}

		char *oriP2 = p2;
		MsHttpMsg msg;

		msg.Parse(p2);

		int cntLen = msg.m_contentLength.GetIntVal();
		// if cntLen over 1.5GB, reject
		if (cntLen < 0 || cntLen > 0x60000000) {
			MS_LOG_ERROR("content len err:%d", cntLen);
			m_bufOff = 0;
			m_server->DelEvent(evt);
			return;
		}

		int left = m_bufOff - (p2 - oriP2);

		if (left < cntLen) {
			int headrLen = p2 - oriP2;
			p2 = oriP2;

			if (m_bufOff && p2 != m_bufPtr.get()) {
				MS_LOG_DEBUG("http buf left:%d", m_bufOff);
				memmove(m_bufPtr.get(), p2, m_bufOff);
			}

			if (headrLen + cntLen >= m_bufSize) {
				int newBufSize = headrLen + cntLen + 1024;
				unique_ptr<char[]> newBufPtr = make_unique<char[]>(newBufSize);
				memcpy(newBufPtr.get(), m_bufPtr.get(), m_bufOff);

				printf("extend mem %d:%d\n", m_bufSize, newBufSize);

				m_bufSize = newBufSize;
				m_bufPtr = std::move(newBufPtr);
			}

			return;
		}

		MS_LOG_DEBUG("recv:%s", oriP2);
#if ENABLE_RTC
		// if msg.m_uri starts with /rtc/, let RtcServer handle it
		if (msg.m_uri.find("/rtc/") == 0) {
			shared_ptr<SHttpTransferMsg> rtcMsg = make_shared<SHttpTransferMsg>();

			rtcMsg->httpMsg = msg;
			rtcMsg->sock = evt->GetSharedSocket();
			rtcMsg->body = string(p2, cntLen);
			MsMsg msMsg;
			msMsg.m_msgID = MS_RTC_MSG;
			msMsg.m_any = rtcMsg;
			msMsg.m_dstID = 1;
			msMsg.m_dstType = MS_RTC_SERVER;
			m_server->PostMsg(msMsg);

			m_server->DelEvent(evt);
			return;
		}
#endif

		// if uri start with /live, /vod, /gbvod, let HttpStream handle it
		if (msg.m_uri.find("/live") == 0 || msg.m_uri.find("/vod") == 0 ||
		    msg.m_uri.find("/gbvod") == 0) {
			shared_ptr<SHttpTransferMsg> httpMsg = make_shared<SHttpTransferMsg>();
			httpMsg->httpMsg = msg;
			httpMsg->sock = evt->GetSharedSocket();
			httpMsg->body = string(p2, cntLen);
			MsMsg msMsg;
			msMsg.m_msgID = MS_HTTP_STREAM_MSG;
			msMsg.m_any = httpMsg;
			msMsg.m_dstID = 1;
			msMsg.m_dstType = MS_HTTP_STREAM;
			m_server->PostMsg(msMsg);

			m_server->DelEvent(evt);
			return;
		}

		m_server->HandleHttpReq(evt, msg, p2, cntLen);

		p2 += cntLen;
		m_bufOff -= (p2 - oriP2);
	}

	if (m_bufSize > DEF_BUF_SIZE) {
		printf("relase big mem\n");

		m_bufSize = DEF_BUF_SIZE;
		m_bufPtr = make_unique<char[]>(DEF_BUF_SIZE);
	}
}

void MsHttpHandler::HandleClose(shared_ptr<MsEvent> evt) { m_server->DelEvent(evt); }
