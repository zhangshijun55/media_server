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
			it->second->SourceActiveClose();
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

				shared_ptr<SRtcPeerConn> peerConn;
				{
					std::lock_guard<std::mutex> lock(m_mtx);
					auto it = m_pcMap.find(sessionId);
					if (it != m_pcMap.end()) {
						peerConn = it->second;
					}
				}

				track->onMessage(
				    [peerConn](rtc::binary message) {
					    if (peerConn) {
						    auto rtp = reinterpret_cast<rtc::RtpHeader *>(message.data());
						    if (rtp->payloadType() == peerConn->_videoPt ||
						        rtp->payloadType() == peerConn->_audioPt) {
							    peerConn->WriteBuffer(message.data(), message.size());
						    }
					    }
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
						peerConn->m_rtpThread =
						    std::make_unique<std::thread>(&SRtcPeerConn::StartRtpDemux, peerConn);
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
				it->second->SourceActiveClose();
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

MsRtcServer::SRtcPeerConn::~SRtcPeerConn() {
	MS_LOG_INFO("~SRtcPeerConn %s", _sessionId.c_str());
	while (m_rtcDataQue.size()) {
		SData data = m_rtcDataQue.front();
		m_rtcDataQue.pop();
		if (data.m_buf) {
			delete[] data.m_buf;
		}
	}
	while (m_recyleQue.size()) {
		SData data = m_recyleQue.front();
		m_recyleQue.pop();
		if (data.m_buf) {
			delete[] data.m_buf;
		}
	}
}

void MsRtcServer::SRtcPeerConn::AddSink(std::shared_ptr<MsMediaSink> sink) {
	if (!sink || m_isClosing.load()) {
		return;
	}

	std::lock_guard<std::mutex> lock(m_sinkMutex);
	m_sinks.push_back(sink);
	if (m_video || m_audio) {
		sink->OnStreamInfo(m_video, m_videoIdx, m_audio, m_audioIdx);
	}
	if (_videoTrack && _pc) {
		_videoTrack->requestKeyframe();
	}
}

void MsRtcServer::SRtcPeerConn::NotifyStreamPacket(AVPacket *pkt) {
	std::lock_guard<std::mutex> lock(m_sinkMutex);
	for (auto &sink : m_sinks) {
		if (sink) {
			sink->OnStreamPacket(pkt);
		}
	}
}

void MsRtcServer::SRtcPeerConn::SourceActiveClose() {
	m_isClosing.store(true);
	m_condVar.notify_all();

	try {
		_pc->close();
	} catch (...) {
	}

	if (m_rtpThread) {
		m_rtpThread->join();
		m_rtpThread.reset();
	}

	if (_sock) {
		_sock.reset();
	}

	MsMediaSource::SourceActiveClose();
}

void MsRtcServer::SRtcPeerConn::GenerateSdp() {
	std::stringstream ss;
	ss << "v=0\r\n";
	ss << "o=- 0 0 IN IP4 127.0.0.1\r\n";
	ss << "c=IN IP4 127.0.0.1\r\n";

	if (_videoPt > 0) {
		ss << "m=video 0 RTP/AVP " << _videoPt << "\r\n";
		ss << "a=rtpmap:" << _videoPt << " " << _videoCodec << "/90000\r\n";
		for (size_t i = 0; i < _videoFmts.size(); ++i) {
			ss << "a=fmtp:" << _videoPt << " " << _videoFmts[i];
			ss << "\r\n";
		}
	}

	if (_audioPt > 0) {
		ss << "m=audio 0 RTP/AVP " << _audioPt << "\r\n";
		int clockRate = 48000;
		ss << "a=rtpmap:" << _audioPt << " " << _audioCodec << "/" << clockRate << "/2\r\n";
		for (size_t i = 0; i < _audioFmts.size(); ++i) {
			ss << "a=fmtp:" << _audioPt << " " << _audioFmts[i];
			ss << "\r\n";
		}
	}

	_sdp = ss.str();
	MS_LOG_DEBUG("pc:%s generated sdp:\n%s", _sessionId.c_str(), _sdp.c_str());
}

void MsRtcServer::SRtcPeerConn::StartRtpDemux() {
	int rtp_buff_size = 1500;
	AVIOContext *rtp_avio_context = nullptr;
	AVIOContext *sdp_avio_context = nullptr;
	AVFormatContext *fmtCtx = nullptr;
	AVPacket *pkt = nullptr;
	bool fisrtVideoPkt = true;

	this->GenerateSdp();
	MsResManager::GetInstance().AddMediaSource(_sessionId, this->GetSharedPtr());

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
		    return rpc->ReadBuffer(buf, buf_size);
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

	ret = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
	if (ret < 0) {
		MS_LOG_ERROR("pc:%s no video stream found", _sessionId.c_str());
		goto err;
	}
	m_videoIdx = ret;
	m_video = fmtCtx->streams[m_videoIdx];

	ret = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
	if (ret >= 0) {
		AVCodecParameters *codecPar = fmtCtx->streams[ret]->codecpar;
		if (codecPar->codec_id == AV_CODEC_ID_OPUS || codecPar->codec_id == AV_CODEC_ID_AAC) {
			m_audioIdx = ret;
			m_audio = fmtCtx->streams[m_audioIdx];
		}
	}

	this->NotifyStreamInfo();

	pkt = av_packet_alloc();

	/* read frames from the file */
	while (av_read_frame(fmtCtx, pkt) >= 0 && !m_isClosing.load()) {
		if (pkt->stream_index == m_videoIdx || pkt->stream_index == m_audioIdx) {
			// if (pkt->stream_index == m_videoIdx) {
			// 	MS_LOG_DEBUG("rtc source video pkt pts:%lld dts:%lld key:%d", pkt->pts, pkt->dts,
			// 	             pkt->flags & AV_PKT_FLAG_KEY);
			// } else if (pkt->stream_index == m_audioIdx) {
			// 	MS_LOG_DEBUG("rtc source audio pkt pts:%lld dts:%lld size:%d", pkt->pts, pkt->dts,
			// 	             pkt->size);
			// }

			if (pkt->stream_index == m_videoIdx) {
				if (fisrtVideoPkt) {
					fisrtVideoPkt = false;
					// TODO: quick fix for some RTSP stream with first pkt pts=dts=AV_NOPTS_VALUE
					//       need better solution
					if (pkt->pts == AV_NOPTS_VALUE) {
						MS_LOG_WARN("first video pkt pts is AV_NOPTS_VALUE, set to 0");
						pkt->pts = 0;
						if (pkt->dts == AV_NOPTS_VALUE) {
							pkt->dts = 0;
						}
					}
				}
			}

			this->NotifyStreamPacket(pkt);
		}
		av_packet_unref(pkt);
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

	av_packet_free(&pkt);

	MsMsg msg;
	msg.m_msgID = MS_RTC_PEER_CLOSED;
	msg.m_strVal = _sessionId;
	msg.m_dstType = MS_RTC_SERVER;
	msg.m_dstID = 1;
	MsReactorMgr::Instance()->PostMsg(msg);
}

void MsRtcServer::SRtcPeerConn::WriteBuffer(const void *buf, int size) {
	if (m_isClosing.load()) {
		return;
	}

	std::unique_lock<std::mutex> lock(m_queMtx);
	if (m_recyleQue.size() > 0) {
		SData &sd = m_recyleQue.front();
		if (sd.m_capacity >= size) {
			memcpy(sd.m_buf, buf, size);
			sd.m_len = size;
			m_rtcDataQue.push(sd);
			m_recyleQue.pop();
			lock.unlock();
			m_condVar.notify_one();
			return;
		} else {
			// not enough capacity
			delete[] sd.m_buf;
			m_recyleQue.pop();
		}
	}

	SData sd;
	sd.m_buf = new uint8_t[size];
	sd.m_len = size;
	sd.m_capacity = size;
	memcpy(sd.m_buf, buf, size);
	m_rtcDataQue.push(sd);
	lock.unlock();
	m_condVar.notify_one();
}

int MsRtcServer::SRtcPeerConn::ReadBuffer(uint8_t *buf, int buf_size) {
	std::unique_lock<std::mutex> lock(m_queMtx);
	m_condVar.wait(lock, [this]() { return m_rtcDataQue.size() > 0 || m_isClosing.load(); });

	if (m_isClosing.load()) {
		return AVERROR_EOF;
	}

	if (m_rtcDataQue.size() <= 0) {
		return AVERROR(EAGAIN);
	}

	SData &sd = m_rtcDataQue.front();
	int toRead = buf_size;
	if (toRead > sd.m_len) {
		toRead = sd.m_len;
	}

	memcpy(buf, sd.m_buf, toRead);
	if (toRead < sd.m_len) {
		memmove(sd.m_buf, sd.m_buf + toRead, sd.m_len - toRead);
	}
	sd.m_len -= toRead;

	if (sd.m_len == 0) {
		m_recyleQue.push(sd);
		m_rtcDataQue.pop();
	}

	return toRead;
}
