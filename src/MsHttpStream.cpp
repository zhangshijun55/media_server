#include "MsHttpStream.h"
#include "MsConfig.h"
#include "MsDevMgr.h"
#include "MsHttpSink.h"
#include "MsLog.h"
#include "MsMsgDef.h"
#include "MsPortAllocator.h"
#include "MsResManager.h"
#include "MsSourceFactory.h"
#include "nlohmann/json.hpp"
#include <thread>

using json = nlohmann::json;

void MsHttpStream::HandleMsg(MsMsg &msg) {
	switch (msg.m_msgID) {
	case MS_HTTP_STREAM_MSG: {
		SHttpTransferMsg *httpMsg = static_cast<SHttpTransferMsg *>(msg.m_ptr);
		this->HandleStreamMsg(httpMsg);
		delete httpMsg;
	} break;
	default:
		MsReactor::HandleMsg(msg);
		break;
	}
}

void MsHttpStream::HandleStreamMsg(SHttpTransferMsg *httpMsg) {
	MsHttpMsg &msg = httpMsg->httpMsg;
	shared_ptr<MsSocket> &sock = httpMsg->sock;

	std::vector<std::string> s = SplitString(msg.m_uri, "/");
	if (s.size() < 3) {
		MS_LOG_WARN("invalid uri:%s", msg.m_uri.c_str());
		json rsp;
		rsp["code"] = 1;
		rsp["msg"] = "invalid uri";
		SendHttpRsp(sock.get(), rsp.dump());
		return;
	}

	if (s[1] == "live") {
		std::vector<std::string> params = SplitString(s[2], ".");
		std::string streamID = params[0];
		std::string format = (params.size() > 1) ? params[1] : "flv";

		if (format != "flv" && format != "ts") {
			MS_LOG_WARN("unsupported format:%s", format.c_str());
			json rsp;
			rsp["code"] = 1;
			rsp["msg"] = "unsupported format";
			SendHttpRsp(sock.get(), rsp.dump());
			return;
		}

		std::shared_ptr<MsMediaSink> sink =
		    std::make_shared<MsHttpSink>(format, streamID, ++m_seqID, sock);

		std::shared_ptr<MsMediaSource> source =
		    MsResManager::GetInstance().GetOrCreateMediaSource(s[1], streamID, "", sink);

		if (!source) {
			MS_LOG_WARN("create source failed for stream: %s", streamID.c_str());
			json rsp;
			rsp["code"] = 1;
			rsp["msg"] = "create source failed";
			SendHttpRsp(sock.get(), rsp.dump());
			return;
		}

	} else if (s[1] == "vod") {
		if (s.size() < 5) {
			MS_LOG_WARN("invalid vod uri:%s", msg.m_uri.c_str());
			json rsp;
			rsp["code"] = 1;
			rsp["msg"] = "invalid uri";
			SendHttpRsp(sock.get(), rsp.dump());
			return;
		}

		std::string streamID = s[2];
		std::string format = s[3];
		std::string filename = s[4];

		std::shared_ptr<MsMediaSink> sink =
		    std::make_shared<MsHttpSink>(format, streamID, ++m_seqID, sock);

		std::shared_ptr<MsMediaSource> source =
		    MsResManager::GetInstance().GetOrCreateMediaSource(s[1], streamID, filename, sink);

		if (!source) {
			MS_LOG_WARN("create source failed for stream: %s", streamID.c_str());
			json rsp;
			rsp["code"] = 1;
			rsp["msg"] = "create source failed";
			SendHttpRsp(sock.get(), rsp.dump());
			return;
		}

	} else if (s[1] == "gbvod") {
		if (s.size() < 4) {
			MS_LOG_WARN("invalid gbvod uri:%s", msg.m_uri.c_str());
			json rsp;
			rsp["code"] = 1;
			rsp["msg"] = "invalid uri";
			SendHttpRsp(sock.get(), rsp.dump());
			return;
		}

		std::string streamID = s[2];
		std::vector<std::string> fmtInfo = SplitString(s[3], ".");
		std::string streamInfo = fmtInfo[0];
		std::string format = (fmtInfo.size() > 1) ? fmtInfo[1] : "flv";

		if (format != "flv" && format != "ts") {
			MS_LOG_WARN("unsupported format:%s", format.c_str());
			json rsp;
			rsp["code"] = 1;
			rsp["msg"] = "unsupported format";
			SendHttpRsp(sock.get(), rsp.dump());
			return;
		}

		std::shared_ptr<MsMediaSink> sink =
		    std::make_shared<MsHttpSink>(format, streamID, ++m_seqID, sock);

		std::shared_ptr<MsMediaSource> source =
		    MsResManager::GetInstance().GetOrCreateMediaSource(s[1], streamID, streamInfo, sink);

		if (!source) {
			MS_LOG_WARN("create source failed for stream: %s", streamID.c_str());
			json rsp;
			rsp["code"] = 1;
			rsp["msg"] = "create source failed";
			SendHttpRsp(sock.get(), rsp.dump());
			return;
		}

	} else {
		MS_LOG_WARN("unsupported uri:%s", msg.m_uri.c_str());
		json rsp;
		rsp["code"] = 1;
		rsp["msg"] = "unsupported uri";
		SendHttpRsp(sock.get(), rsp.dump());
	}
}
