#include "MsRtmpServer.h"
#include "MsConfig.h"
#include "MsHttpMsg.h"
#include "MsLog.h"
#include "MsMsgDef.h"
#include "MsRtmpHandler.h"
#include "nlohmann/json.hpp"
#include <string.h>

using json = nlohmann::json;

void MsRtmpServer::HandleMsg(MsMsg &msg) {
	switch (msg.m_msgID) {
	case MS_SOCK_TRANSFER_MSG: {
		// Handle RTMP socket transfer message
		auto rtmpMsg = std::any_cast<shared_ptr<SSockTransferMsg>>(msg.m_any);
		shared_ptr<MsRtmpHandler> handler =
		    make_shared<MsRtmpHandler>(dynamic_pointer_cast<MsRtmpServer>(shared_from_this()));
		shared_ptr<MsEvent> event =
		    make_shared<MsEvent>(rtmpMsg->sock, MS_FD_READ | MS_FD_CLOSE, handler);
		this->AddEvent(event);

		// copy data to handler buffer and process
		if (rtmpMsg->data.size() > handler->m_bufferSize) {
			handler->m_buffer = make_unique<uint8_t[]>(rtmpMsg->data.size());
			handler->m_bufferSize = rtmpMsg->data.size();
		}

		memcpy(handler->m_buffer.get(), rtmpMsg->data.data(), rtmpMsg->data.size());
		handler->m_dataLength = rtmpMsg->data.size();
		handler->m_recvBytes = rtmpMsg->data.size();
		handler->ProcessBuffer(event);
	} break;

	case MS_HTTP_TRANSFER_MSG: {
		auto httpMsg = std::any_cast<shared_ptr<SHttpTransferMsg>>(msg.m_any);
		string uri = httpMsg->httpMsg.m_uri;
		if (uri.find("/rtmp/stream/url") == 0) {
			string stream;
			GetParam("stream", stream, uri);
			shared_ptr<MsMediaSource> source = GetRtmpSource(stream);
			json j;
			if (source) {
#if ENABLE_HTTPS
				string protocol = "https://";
#else
				string protocol = "http://";
#endif

				string httpIp = MsConfig::Instance()->GetConfigStr("localBindIP");
				int httpPort = MsConfig::Instance()->GetConfigInt("httpPort");
				string sessionId = source->GetStreamID();

				json result;
				char bb[512];
				sprintf(bb, "%s%s:%d/live/%s.flv", protocol.c_str(), httpIp.c_str(), httpPort,
				        sessionId.c_str());
				result["httpFlvUrl"] = bb;

				sprintf(bb, "%s%s:%d/live/%s.ts", protocol.c_str(), httpIp.c_str(), httpPort,
				        sessionId.c_str());
				result["httpTsUrl"] = bb;

				sprintf(bb, "%s%s:%d/rtc/whep/%s", protocol.c_str(), httpIp.c_str(), httpPort,
				        sessionId.c_str());
				result["rtcUrl"] = bb;

				sprintf(bb, "rtsp://%s:%d/live/%s", httpIp.c_str(), httpPort, sessionId.c_str());
				result["rtspUrl"] = bb;

				j["code"] = 0;
				j["msg"] = "success";
				j["result"] = result;
			} else {
				j["code"] = 1;
				j["message"] = "Stream not found";
			}
			SendHttpRspEx(httpMsg->sock.get(), j.dump());

		} else if (uri.find("/rtmp/stream") == 0) {
			json j, streams = json::array();
			for (const auto &it : m_rtmpSources) {
				json stream;
				stream["stream"] = it.first;
				stream["videoCodec"] = it.second->GetVideoCodec();
				stream["audioCodec"] = it.second->GetAudioCodec();
				streams.push_back(stream);
			}
			j["result"] = streams;
			j["code"] = 0;
			j["message"] = "OK";
			SendHttpRspEx(httpMsg->sock.get(), j.dump());
		} else {
			MsHttpMsg rsp;
			rsp.m_version = httpMsg->httpMsg.m_version;
			rsp.m_status = "404";
			rsp.m_reason = "Not Found";
			SendHttpRspEx(httpMsg->sock.get(), rsp);
		}

	} break;

	default:
		MsReactor::HandleMsg(msg);
		break;
	}
}

void MsRtmpServer::RegistSource(const string &streamID, shared_ptr<MsMediaSource> source) {
	m_rtmpSources[streamID] = source;
}

void MsRtmpServer::UnregistSource(const string &streamID) { m_rtmpSources.erase(streamID); }

shared_ptr<MsMediaSource> MsRtmpServer::GetRtmpSource(const string &streamID) {
	auto it = m_rtmpSources.find(streamID);

	if (it != m_rtmpSources.end()) {
		return it->second;
	}
	return nullptr;
}
