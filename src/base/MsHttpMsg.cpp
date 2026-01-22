#include "MsHttpMsg.h"
#include "MsLog.h"
#include "MsSocket.h"

MsHttpMsg::MsHttpMsg()
    : m_connection("Connection"), m_host("Host"), m_allowOrigin("Access-Control-Allow-Origin"),
      m_allowMethod("Access-Control-Allow-Methods"), m_allowHeader("Access-Control-Allow-Headers"),
      m_exposeHeader("Access-Control-Expose-Headers"), m_transport("Transfer-Encoding"),
      m_location("Location"), m_allowPrivateNetwork("Access-Control-Allow-Private-Network") {}

void MsHttpMsg::Dump(string &rsp) {
	if (m_status.size()) {
		BuildFirstLine(rsp, m_version, m_status, m_reason);
	} else {
		BuildFirstLine(rsp, m_method, m_uri, m_version);
	}

	m_host.Dump(rsp);
	if (!m_transport.m_exist)
		m_contentLength.Dump(rsp);
	m_contentType.Dump(rsp);
	m_connection.Dump(rsp);
	m_location.Dump(rsp);
	m_transport.Dump(rsp);
	m_allowOrigin.Dump(rsp);
	m_allowMethod.Dump(rsp);
	m_allowHeader.Dump(rsp);
	m_exposeHeader.Dump(rsp);
	m_allowPrivateNetwork.Dump(rsp);

	rsp += "\r\n";

	if (m_body && m_bodyLen) {
		rsp.append(m_body, m_bodyLen);
	}
}

void MsHttpMsg::Parse(char *&p2) {
	ParseReqLine(p2, m_method, m_uri, m_version);

	if (m_method == "HTTP/1.1") {
		m_status = m_uri;
		m_reason = m_version;
		m_version = m_method;
	}

	string line;
	string key, value;

	while (GetHeaderLine(p2, line)) {
		size_t p1 = line.find_first_of(':');
		size_t p2 = line.find_first_not_of(' ', p1 + 1);

		if (p2 == string::npos) {
			continue;
		}

		key = line.substr(0, p1);
		value = line.substr(p2);

		if (!strcasecmp(key.c_str(), "Content-Length")) {
			m_contentLength.SetValue(value);
		} else if (!strcasecmp(key.c_str(), "Content-Type")) {
			m_contentType.SetValue(value);
		} else if (!strcasecmp(key.c_str(), "Host")) {
			m_host.SetValue(value);
		} else if (!strcasecmp(key.c_str(), "Transfer-Encoding")) {
			m_transport.SetValue(value);
		}
	}
}

void SendHttpRsp(MsSocket *sock, const string &rspBody) {
	MsHttpMsg rsp;

	rsp.m_version = "HTTP/1.1";
	rsp.m_status = "200";
	rsp.m_reason = "OK";
	rsp.m_connection.SetValue("close");
	rsp.m_allowOrigin.SetValue("*");
	rsp.m_allowMethod.SetValue("GET, POST, OPTIONS, DELETE");
	rsp.m_allowHeader.SetValue(
	    "DNT,X-Mx-ReqToken,Keep-Alive,User-Agent,X-Requested-With,If-Modified-"
	    "Since,Cache-Control,Content-Type,Authorization,Location");
	rsp.m_contentType.SetValue("application/json; charset=UTF-8");
	rsp.SetBody(rspBody.c_str(), rspBody.size());

	SendHttpRsp(sock, rsp);
}

void SendHttpRspEx(MsSocket *sock, const string &rspBody) {
	MsHttpMsg rsp;

	rsp.m_version = "HTTP/1.1";
	rsp.m_status = "200";
	rsp.m_reason = "OK";
	rsp.m_connection.SetValue("close");
	rsp.m_contentType.SetValue("application/json; charset=UTF-8");
	rsp.m_allowOrigin.SetValue("*");
	rsp.m_allowMethod.SetValue("GET, POST, OPTIONS, DELETE");
	rsp.m_allowHeader.SetValue(
	    "DNT,X-Mx-ReqToken,Keep-Alive,User-Agent,X-Requested-With,If-Modified-"
	    "Since,Cache-Control,Content-Type,Authorization,Location");

	rsp.SetBody(rspBody.c_str(), rspBody.size());

	SendHttpRsp(sock, rsp);
}

void SendHttpRsp(MsSocket *sock, MsHttpMsg &rsp) {
	string strRsp;
	rsp.Dump(strRsp);

	int ret = sock->Send(strRsp.c_str(), strRsp.size());

	MS_LOG_DEBUG("send[%d:%d]:%s", ret, strRsp.size(), strRsp.c_str());
}

void SendHttpRspEx(MsSocket *sock, MsHttpMsg &rsp) {

	rsp.m_connection.SetValue("close");
	if (!rsp.m_contentType.m_exist) {
		rsp.m_contentType.SetValue("application/json; charset=UTF-8");
	}
	rsp.m_allowPrivateNetwork.SetValue("true");
	rsp.m_allowOrigin.SetValue("*");
	rsp.m_allowMethod.SetValue("GET, POST, OPTIONS, DELETE");
	rsp.m_allowHeader.SetValue(
	    "DNT,X-Mx-ReqToken,Keep-Alive,User-Agent,X-Requested-With,If-Modified-"
	    "Since,Cache-Control,Content-Type,Authorization,Location,access-control-request-private-"
	    "network,content-type");

	SendHttpRsp(sock, rsp);
}
