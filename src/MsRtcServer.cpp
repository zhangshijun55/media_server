#include "MsRtcServer.h"
#include "MsCommon.h"
#include "MsConfig.h"

extern "C" {
#include <libavformat/avformat.h>
}

void MsRtcServer::Run() {
	this->RegistToManager();

	rtc::InitLogger(rtc::LogLevel::Debug);

	std::thread worker(&MsReactor::Wait, shared_from_this());
	worker.detach();
}

void MsRtcServer::HandleMsg(MsMsg &msg) {
	switch (msg.m_msgID) {
	case MS_RTC_MSG: {
		SHttpTransferMsg *rtcMsg = static_cast<SHttpTransferMsg *>(msg.m_ptr);
		this->RtcProcess(rtcMsg);
		delete rtcMsg;
	} break;

	case MS_RTC_PEER_CLOSED: {
		string sessionId = msg.m_strVal;
		std::lock_guard<std::mutex> lock(m_mtx);
		auto it = m_pcMap.find(sessionId);
		if (it != m_pcMap.end()) {
			m_pcMap.erase(it);
			MS_LOG_INFO("pc:%s removed", sessionId.c_str());
		}
	} break;

	case MS_RTC_DEL_SOCK: {
		string sessionId = msg.m_strVal;
		std::lock_guard<std::mutex> lock(m_mtx);
		auto it = m_pcMap.find(sessionId);
		if (it != m_pcMap.end()) {
			it->second->_sock.reset();
			MS_LOG_INFO("pc:%s sock reset", sessionId.c_str());
		}
	} break;

	default:
		MsReactor::HandleMsg(msg);
		break;
	}
}

void MsRtcServer::RtcProcess(SHttpTransferMsg *rtcMsg) {
	// if httpMsg.uri contains "whip" then whip process else return 404
	if (rtcMsg->httpMsg.m_uri.find("whip") != string::npos) {
		return this->WhipProcess(rtcMsg);
	} else {
		MsHttpMsg rsp;
		rsp.m_version = rtcMsg->httpMsg.m_version;
		rsp.m_status = "404";
		rsp.m_reason = "Not Found";
		SendHttpRspEx(rtcMsg->sock.get(), rsp);
	}
}

void MsRtcServer::WhipProcess(SHttpTransferMsg *rtcMsg) {
	MsHttpMsg &msg = rtcMsg->httpMsg;
	shared_ptr<MsSocket> &sock = rtcMsg->sock;
	string &sdp = rtcMsg->body;

	// if httpMsg.method is OPTIONS or GET return 200 OK with allow headers
	if (msg.m_method == "OPTIONS" || msg.m_method == "GET") {
		MsHttpMsg rsp;
		rsp.m_version = msg.m_version;
		rsp.m_status = "204";
		rsp.m_reason = "No Content";
		SendHttpRspEx(sock.get(), rsp);
	} else if (msg.m_method == "POST") {
		string sessionId = GenRandStr(16);
		string version = msg.m_version;
		string httpIp = MsConfig::Instance()->GetConfigStr("localBindIP");
		int httpPort = MsConfig::Instance()->GetConfigInt("httpPort");

#if ENABLE_HTTPS
		string protocol = "https://";
#else
		string protocol = "http://";
#endif

		string location =
		    protocol + httpIp + ":" + to_string(httpPort) + msg.m_uri + "/" + sessionId;

		rtc::Configuration config;
		config.enableIceUdpMux = true;
		config.portRangeBegin = 26090;
		config.portRangeEnd = 26090;

		auto pc = make_shared<rtc::PeerConnection>(config);

		int expectedTracks = 0;
		rtc::Description offer(sdp, "offer");
		for (int i = 0; i < offer.mediaCount(); ++i) {
			auto var = offer.media(i);
			auto media = std::get_if<rtc::Description::Media *>(&var);
			if (media == nullptr) {
				continue;
			}
			string type = (*media)->type();
			if (type == "audio" || type == "video") {
				expectedTracks++;
			}
		}

		shared_ptr<SRtcPeerConn> peerConn = make_shared<SRtcPeerConn>(sessionId);
		peerConn->_pc = pc;
		peerConn->_sock = sock;
		peerConn->_sessionId = sessionId;
		peerConn->_expectedTracks = expectedTracks;

		{
			std::lock_guard<std::mutex> lock(m_mtx);
			m_pcMap[sessionId] = peerConn;
		}

		pc->onStateChange([this, sessionId](rtc::PeerConnection::State state) {
			// just log state change, and log state string
			string strState;
			switch (state) {
			case rtc::PeerConnection::State::New:
				strState = "New";
				break;
			case rtc::PeerConnection::State::Connecting:
				strState = "Connecting";
				break;
			case rtc::PeerConnection::State::Connected:
				strState = "Connected";
				break;
			case rtc::PeerConnection::State::Disconnected:
				strState = "Disconnected";
				break;
			case rtc::PeerConnection::State::Failed:
				strState = "Failed";
				break;
			case rtc::PeerConnection::State::Closed: {
				strState = "Closed";
				// callback must not use lock
				MsMsg msg;
				msg.m_msgID = MS_RTC_PEER_CLOSED;
				msg.m_strVal = sessionId;
				this->EnqueMsg(msg);
			} break;
			}
			MS_LOG_INFO("pc:%s state:%s", sessionId.c_str(), strState.c_str());
		});

		pc->onTrack([this, sessionId](shared_ptr<rtc::Track> track) {
			weak_ptr<rtc::Track> weak_track = track;
			track->onOpen([weak_track, sessionId]() {
				auto track = weak_track.lock();
				if (track)
					MS_LOG_INFO("pc:%s track open:%s", sessionId.c_str(), track->mid().c_str());
			});

			rtc::Description::Media media = track->description();
			string mediaType = media.type();
			MS_LOG_INFO("pc:%s track %s type:%s", sessionId.c_str(), track->mid().c_str(),
			            mediaType.c_str());

			auto payloadTypes = media.payloadTypes();
			// use 1 bool is enough since we only accept one video and one audio track
			bool mediaFound = false;

			for (auto &pt : payloadTypes) {
				rtc::Description::Media::RtpMap *rtpMap = nullptr;
				try {
					rtpMap = media.rtpMap(pt);
				} catch (...) {
					continue;
				}

				if (mediaFound) {
					if (rtpMap->format != "rtx" && rtpMap->format != "red" &&
					    rtpMap->format != "ulpfec") {
						// only accept one media track, remove other rtpmap except rtx
						media.removeRtpMap(pt);
						MS_LOG_INFO("pc:%s %s track remove extra format:%s payload:%d",
						            sessionId.c_str(), mediaType.c_str(), rtpMap->format.c_str(),
						            pt);
					} else {
						// for rtx, red, ulpfec just keep it
					}

					continue;
				}

				if (rtpMap->format == "OPUS" || rtpMap->format == "opus" ||
				    rtpMap->format == "H264" || rtpMap->format == "H265") {
					mediaFound = true;

					MS_LOG_INFO("pc:%s %s track format:%s payload:%d", sessionId.c_str(),
					            mediaType.c_str(), rtpMap->format.c_str(), pt);

					std::lock_guard<std::mutex> lock(m_mtx);
					auto it = m_pcMap.find(sessionId);
					if (it != m_pcMap.end()) {
						auto &peerConn = it->second;
						if (mediaType == "video" && !peerConn->_videoTrack) {
							peerConn->_videoTrack = track;
							peerConn->_videoPt = pt;
							peerConn->_videoCodec = rtpMap->format;
							peerConn->_videoFmts = rtpMap->fmtps;
						} else if (mediaType == "audio" && !peerConn->_audioTrack) {
							peerConn->_audioTrack = track;
							peerConn->_audioPt = pt;
							peerConn->_audioCodec = rtpMap->format;
							peerConn->_audioFmts = rtpMap->fmtps;
						}
					}

				} else {
					media.removeRtpMap(pt);
					MS_LOG_INFO("pc:%s %s track remove unsupported format:%s payload:%d",
					            sessionId.c_str(), mediaType.c_str(), rtpMap->format.c_str(), pt);
				}

				std::shared_ptr<rtc::RtcpReceivingSession> session =
				    std::make_shared<rtc::RtcpReceivingSession>();
				track->setMediaHandler(session);

				track->onMessage(
				    [](rtc::binary message) {
					    // This is an RTP packet
					    auto rtp = reinterpret_cast<rtc::RtpHeader *>(message.data());
					    // log payload type and size
					    printf("rtp payload type:%d size:%d\n", rtp->payloadType(), message.size());
				    },
				    nullptr);
			}

			track->setDescription(std::move(media));

			{
				std::lock_guard<std::mutex> lock(m_mtx);
				auto it = m_pcMap.find(sessionId);
				if (it != m_pcMap.end()) {
					auto &peerConn = it->second;
					peerConn->_receivedTracks++;
					if (peerConn->_receivedTracks == peerConn->_expectedTracks) {
						MS_LOG_INFO("pc:%s all %d tracks received", sessionId.c_str(),
						            peerConn->_expectedTracks);
					}
				}
			}
		});

		pc->onLocalDescription([](rtc::Description description) {});

		std::weak_ptr<rtc::PeerConnection> weak_pc = pc;
		std::weak_ptr<MsSocket> weak_sock = sock;

		pc->onGatheringStateChange([this, weak_pc, weak_sock, sessionId, version,
		                            location](rtc::PeerConnection::GatheringState state) {
			if (state == rtc::PeerConnection::GatheringState::Complete) {
				auto pc = weak_pc.lock();
				if (!pc)
					return;

				auto sock = weak_sock.lock();
				if (!sock)
					return;

				auto description = pc->localDescription();

				MsHttpMsg rsp;
				rsp.m_version = version;
				rsp.m_status = "201";
				rsp.m_reason = "Created";
				rsp.m_location.SetValue(location);
				rsp.m_contentType.SetValue("application/sdp");
				rsp.m_exposeHeader.SetValue("Location");

				string sdp = string(*description);
				rsp.SetBody(sdp.c_str(), sdp.size());
				SendHttpRspEx(sock.get(), rsp);

				MsMsg msg;
				msg.m_msgID = MS_RTC_DEL_SOCK;
				msg.m_strVal = sessionId;
				this->EnqueMsg(msg);
			}
		});

		try {
			pc->setRemoteDescription(std::move(offer));
		} catch (const std::exception &e) {
			printf("pc:%s gather candidates exception:%s\n", sessionId.c_str(), e.what());
		}

	} else if (msg.m_method == "DELETE") {
		size_t pos = msg.m_uri.find_last_of('/');
		if (pos != string::npos && pos < msg.m_uri.size() - 1) {
			string sessionId = msg.m_uri.substr(pos + 1);
			std::lock_guard<std::mutex> lock(m_mtx);
			auto it = m_pcMap.find(sessionId);
			if (it != m_pcMap.end()) {
				it->second->_pc->close();
				m_pcMap.erase(it);
			}
		}

		MsHttpMsg rsp;
		rsp.m_version = msg.m_version;
		rsp.m_status = "200";
		rsp.m_reason = "OK";
		SendHttpRspEx(sock.get(), rsp);
	} else {
		MsHttpMsg rsp;
		rsp.m_version = msg.m_version;
		rsp.m_status = "405";
		rsp.m_reason = "Method Not Allowed";

		string body = "405 Method Not Allowed";
		rsp.m_contentLength.SetIntVal(body.size());
		rsp.m_body = body.c_str();
		rsp.m_bodyLen = body.size();
		rsp.m_contentType.SetValue("text/plain; charset=UTF-8");

		SendHttpRspEx(sock.get(), rsp);
	}
}

void MsRtcServer::SRtcPeerConn::StartRtpDemux() {
	int rtp_buff_size = 1500;
	AVIOContext *rtp_avio_context = nullptr;
	AVIOContext *sdp_avio_context = nullptr;
	AVFormatContext *fmtCtx = nullptr;

	sdp_avio_context = avio_alloc_context(
	    static_cast<unsigned char *>(av_malloc(_sdp.size())), _sdp.size(), 0, this,
	    [](void *opaque, uint8_t *buf, int buf_size) -> int {
		    auto rpc = static_cast<SRtcPeerConn *>(opaque);
		    if (rpc->_readSdpPos >= (int)rpc->_sdp.size()) {
			    return AVERROR_EOF;
		    }

		    int left = rpc->_sdp.size() - rpc->_readSdpPos;
		    if (buf_size > left) {
			    buf_size = left;
		    }

		    memcpy(buf, rpc->_sdp.c_str() + rpc->_readSdpPos, buf_size);
		    rpc->_readSdpPos += buf_size;
		    return buf_size;
	    },
	    NULL, NULL);

	rtp_avio_context = avio_alloc_context(
	    static_cast<unsigned char *>(av_malloc(rtp_buff_size)), rtp_buff_size, 1, this,
	    [](void *opaque, uint8_t *buf, int buf_size) -> int {
		    // This is RTP Packet
		    auto rpc = static_cast<SRtcPeerConn *>(opaque);
		    //
		    return buf_size;
	    },
	    // Ignore RTCP Packets. Must be set
	    [](void *, const uint8_t *, int buf_size) -> int { return buf_size; }, NULL);

	fmtCtx = avformat_alloc_context();
	fmtCtx->pb = sdp_avio_context;

	AVDictionary *options = nullptr;
	av_dict_set(&options, "sdp_flags", "custom_io", 0);
	av_dict_set_int(&options, "reorder_queue_size", 0, 0);
	av_dict_set(&options, "protocol_whitelist", "file,rtp,udp", 0);

	int ret = avformat_open_input(&fmtCtx, "", nullptr, &options);
	av_dict_free(&options);

	if (ret != 0) {
		MS_LOG_ERROR("pc:%s avformat_open_input failed:%d", _sessionId.c_str(), ret);
		goto err;
	}

	// release sdp avio context
	av_freep(&sdp_avio_context->buffer);
	avio_context_free(&sdp_avio_context);
	sdp_avio_context = nullptr;

	fmtCtx->pb = rtp_avio_context;

	if ((ret = avformat_find_stream_info(fmtCtx, nullptr)) != 0) {
		// log error
		MS_LOG_ERROR("pc:%s avformat_find_stream_info failed:%d", _sessionId.c_str(), ret);
		goto err;
	}

err:
	if (fmtCtx) {
		if (fmtCtx->pb) {
			av_freep(&fmtCtx->pb->buffer);
			avio_context_free(&fmtCtx->pb);
		}
		avformat_free_context(fmtCtx);
		fmtCtx = nullptr;
	}

	return;
}
