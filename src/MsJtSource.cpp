#include "MsJtSource.h"
#include "MsPortAllocator.h"
#include <thread>

class MsJtSourceHandler : public MsEventHandler {
public:
	enum HandlerType { ACCEPT_HANDLER, TCP_DATA_HANDLER, UDP_DATA_HANDLER };

	MsJtSourceHandler(const shared_ptr<MsJtSource> &source, HandlerType type)
	    : m_source(source), m_type(type), m_bufPtr(new uint8_t[4096]), m_bufSize(4096),
	      m_bufOff(0) {}
	~MsJtSourceHandler() { MS_LOG_INFO("~MsJtSourceHandler"); }

	void HandleRead(shared_ptr<MsEvent> evt) override {
		if (m_type == ACCEPT_HANDLER) {
			// Handle new incoming JT TCP connection
			shared_ptr<MsSocket> clientSock;
			if (evt->GetSocket()->Accept(clientSock) == 0) {
				MS_LOG_INFO("JT source accepted new connection, fd=%d", clientSock->GetFd());
				// Add event for the new socket
				shared_ptr<MsEvent> clientEvent = make_shared<MsEvent>(
				    clientSock, MS_FD_READ | MS_FD_CLOSE,
				    make_shared<MsJtSourceHandler>(m_source, TCP_DATA_HANDLER));
				m_source->OnTcpEvent(clientEvent);
			} else {
				MS_LOG_WARN("JT source accept failed");
			}
			m_source->CloseAcceptEvent();
			return;
		}

		if (m_firstRecv) {
			if (m_type == TCP_DATA_HANDLER) {
				m_source->CloseUdpEvent();
				MS_LOG_INFO("JT source TCP ready, fd=%d", evt->GetSocket()->GetFd());
			} else if (m_type == UDP_DATA_HANDLER) {
				m_source->CloseAcceptEvent();
				m_source->CloseTcpEvent();
				MS_LOG_INFO("JT source UDP ready, fd=%d", evt->GetSocket()->GetFd());
			}
			m_firstRecv = false;
		}

		// Handle incoming data from JT source
		MsSocket *sock = evt->GetSocket();
		int n = sock->Recv((char *)m_bufPtr.get() + m_bufOff, m_bufSize - m_bufOff);
		if (n <= 0) {
			MS_LOG_WARN("JT source connection closed or error, fd=%d", sock->GetFd());
			return;
		}
		m_bufOff += n;

		// Process received data (parsing JT packets and forwarding to sinks)
		while (m_bufOff > 4) {
			uint8_t *data = m_bufPtr.get();
			int startPos = -1;
			// Find 0x30 0x31 0x63 0x64
			for (int i = 0; i <= m_bufOff - 4; ++i) {
				if (data[i] == 0x30 && data[i + 1] == 0x31 && data[i + 2] == 0x63 &&
				    data[i + 3] == 0x64) {
					startPos = i;
					break;
				}
			}

			if (startPos == -1) {
				if (m_bufOff > 3) {
					int keep = 3;
					memmove(data, data + m_bufOff - keep, keep);
					m_bufOff = keep;
				}
				break;
			}

			if (startPos > 0) {
				memmove(data, data + startPos, m_bufOff - startPos);
				m_bufOff -= startPos;
			}

			if (m_bufOff < 30) {
				break;
			}

			uint16_t bodyLen = ((uint16_t)data[28] << 8) | data[29];
			int packetLen = 30 + bodyLen;
			if (m_bufOff < packetLen) {
				break;
			}

			uint8_t dataType = (data[15] >> 4) & 0x0F;
			uint16_t seq = ((uint16_t)data[6] << 8) | data[7];
			uint64_t timestamp = 0;
			for (int i = 0; i < 8; i++) {
				timestamp = (timestamp << 8) | data[16 + i];
			}

			char sim[13];
			snprintf(sim, sizeof(sim), "%02x%02x%02x%02x%02x%02x", data[8], data[9], data[10],
			         data[11], data[12], data[13]);

			MS_LOG_INFO("Recv JT1078: sim=%s seq=%d type=%d ts=%lu len=%d", sim, seq, dataType,
			            timestamp, bodyLen);

			m_source->PrepareRtp(dataType, seq, timestamp, data, bodyLen);

			memmove(data, data + packetLen, m_bufOff - packetLen);
			m_bufOff -= packetLen;
		}
	}

	void HandleClose(shared_ptr<MsEvent> evt) override {
		if (m_type == ACCEPT_HANDLER) {
			m_source->CloseAcceptEvent();
		} else if (m_type == TCP_DATA_HANDLER) {
			m_source->CloseTcpEvent();
		} else if (m_type == UDP_DATA_HANDLER) {
			m_source->CloseUdpEvent();
		}
	}

private:
	HandlerType m_type;
	bool m_firstRecv = true;
	shared_ptr<uint8_t[]> m_bufPtr;
	int m_bufSize = 4096;
	int m_bufOff = 0;
	shared_ptr<MsJtSource> m_source;
};

void MsJtSource::Work() {
	auto server =
	    dynamic_pointer_cast<MsJtServer>(MsReactorMgr::Instance()->GetReactor(MS_JT_SERVER, 1));
	if (!server) {
		MS_LOG_ERROR("JT source cannot find JT server reactor");
		return;
	}
	m_server = server;

	string ip;
	int portNum = 0;
	auto tcpSock = MsPortAllocator::Instance()->AllocPort(SOCK_STREAM, ip, portNum);

	auto udpSock = make_shared<MsSocket>(AF_INET, SOCK_DGRAM, 0);
	MsInetAddr udpAddr(AF_INET, ip, portNum);
	udpSock->Bind(udpAddr);

	tcpSock->Listen();
	m_acceptEvent = make_shared<MsEvent>(
	    tcpSock, MS_FD_ACCEPT | MS_FD_CLOSE,
	    make_shared<MsJtSourceHandler>(dynamic_pointer_cast<MsJtSource>(shared_from_this()),
	                                   MsJtSourceHandler::ACCEPT_HANDLER));
	m_server->AddEvent(m_acceptEvent);

	m_udpEvent = make_shared<MsEvent>(
	    udpSock, MS_FD_READ | MS_FD_CLOSE,
	    make_shared<MsJtSourceHandler>(dynamic_pointer_cast<MsJtSource>(shared_from_this()),
	                                   MsJtSourceHandler::UDP_DATA_HANDLER));
	m_server->AddEvent(m_udpEvent);

	m_ip = ip;
	m_port = portNum;

	std::thread worker([this]() { this->OnStreamInfo(m_server->GetStreamInfo(m_terminalId)); });
	worker.detach();
}

void MsJtSource::SourceActiveClose() {
	MsMediaSource::SourceActiveClose();
	this->SourceReleaseRes();
}

void MsJtSource::OnSinksEmpty() {
	MsMediaSource::OnSinksEmpty();
	this->SourceReleaseRes();
}

void MsJtSource::CloseAcceptEvent() {
	if (m_acceptEvent == nullptr) {
		return;
	}
	m_server->DelEvent(m_acceptEvent);
	m_acceptEvent = nullptr;

	if (m_tcpEvent == nullptr && m_udpEvent == nullptr) {
		MS_LOG_INFO("JT source:%s all events closed, source closing", m_streamID.c_str());
		this->SourceActiveClose();
	}
}

void MsJtSource::CloseTcpEvent() {
	if (m_tcpEvent == nullptr) {
		return;
	}
	m_server->DelEvent(m_tcpEvent);
	m_tcpEvent = nullptr;

	if (m_acceptEvent == nullptr && m_udpEvent == nullptr) {
		MS_LOG_INFO("JT source:%s all events closed, source closing", m_streamID.c_str());
		this->SourceActiveClose();
	}
}

void MsJtSource::CloseUdpEvent() {
	if (m_udpEvent == nullptr) {
		return;
	}
	m_server->DelEvent(m_udpEvent);
	m_udpEvent = nullptr;

	if (m_acceptEvent == nullptr && m_tcpEvent == nullptr) {
		MS_LOG_INFO("JT source:%s all events closed, source closing", m_streamID.c_str());
		this->SourceActiveClose();
	}
}

void MsJtSource::OnTcpEvent(shared_ptr<MsEvent> evt) {
	if (m_tcpEvent != nullptr) {
		// this is highly unexpected
		MS_LOG_WARN("JT source TCP event is not null");
		m_server->DelEvent(m_tcpEvent);
		m_tcpEvent = nullptr;
	}

	m_tcpEvent = evt;
	m_server->AddEvent(m_tcpEvent);
}

void MsJtSource::PrepareRtp(uint8_t dataType, uint16_t seq, uint64_t timestamp, uint8_t *data,
                            int bodyLen) {
	if (!m_demuxReady) {
		// this is highly unexpected
		MS_LOG_ERROR("JT source:%s demuxer not ready, cannot prepare RTP", m_streamID.c_str());
		return;
	}

	if (m_isClosing.load()) {
		return;
	}

	uint8_t pt = data[5] & 0x7F;
	if (pt != m_oriVideoPt && pt != m_oriAudioPt) {
		return;
	} else if (pt == m_oriAudioPt && m_audioPt == 0) {
		return;
	}

	bool isVideo = (pt == m_oriVideoPt);
	uint8_t subPkg = data[15] & 0x0F;
	bool marker = (subPkg == 0 || subPkg == 2);

	int rtpLen = 12 + bodyLen;
	SData rtpData;
	if (m_recyleQue.size() > 0) {
		{
			std::lock_guard<std::mutex> lock(m_queMtx);
			rtpData = std::move(m_recyleQue.front());
			m_recyleQue.pop();
		}

		if (rtpData.m_capacity < rtpLen) {
			rtpData.m_uBuf.reset(new uint8_t[rtpLen]);
			rtpData.m_capacity = rtpLen;
		}
	} else {
		rtpData.m_uBuf.reset(new uint8_t[rtpLen]);
		rtpData.m_capacity = rtpLen;
	}

	uint8_t *rtpBuf = rtpData.m_uBuf.get();
	rtpData.m_len = rtpLen;

	rtpBuf[0] = 0x80;
	rtpBuf[1] = (isVideo ? m_videoPt : m_audioPt) & 0x7F;
	if (marker)
		rtpBuf[1] |= 0x80;

	rtpBuf[2] = (seq >> 8) & 0xFF;
	rtpBuf[3] = seq & 0xFF;

	uint32_t ts = timestamp & 0xFFFFFFFF;
	if (isVideo) {
		ts *= 90;
	} else {
		ts *= m_audioClock;
	}

	rtpBuf[4] = (ts >> 24) & 0xFF;
	rtpBuf[5] = (ts >> 16) & 0xFF;
	rtpBuf[6] = (ts >> 8) & 0xFF;
	rtpBuf[7] = ts & 0xFF;

	uint32_t ssrc = 0x12345678;
	rtpBuf[8] = (ssrc >> 24) & 0xFF;
	rtpBuf[9] = (ssrc >> 16) & 0xFF;
	rtpBuf[10] = (ssrc >> 8) & 0xFF;
	rtpBuf[11] = ssrc & 0xFF;

	memcpy(rtpBuf + 12, data + 30, bodyLen);

	std::unique_lock<std::mutex> lock(m_queMtx);
	m_rtpDataQue.emplace(std::move(rtpData));
	lock.unlock();
	m_condVar.notify_one();
}

void MsJtSource::OnStreamInfo(shared_ptr<SJtStreamInfo> avAttr) {
	if (avAttr == nullptr) {
		MS_LOG_ERROR("JT source:%s received null AV attributes", m_streamID.c_str());
		this->SourceActiveClose();
		return;
	}

	m_oriVideoPt = avAttr->m_videoCodec;
	m_oriAudioPt = avAttr->m_audioCodec;

	if (avAttr->m_videoCodec == 98 || avAttr->m_videoCodec == 99) {
		m_videoPt = avAttr->m_videoCodec;
		m_videoCodec = avAttr->m_videoCodec == 98 ? "H264" : "H265";
	}

	if (avAttr->m_audioCodec == 19) {
		m_audioPt = 102;
		m_audioChannels = avAttr->m_audioChannels;
		m_audioCodec = "AAC";
		switch (avAttr->m_audioSampleRate) {
		case 0:
			m_audioClock = 8.0;
			break;
		case 1:
			m_audioClock = 22.05;
			break;
		case 2:
			m_audioClock = 44.1;
			break;
		case 3:
			m_audioClock = 48.0;
			break;
		default:
			m_audioClock = 48.0;
			break;
		}
	}

	if (m_videoPt == 0) {
		MS_LOG_ERROR("JT source:%s unsupported video codec:%d", m_streamID.c_str(),
		             avAttr->m_videoCodec);
		this->SourceActiveClose();
		return;
	}

	m_demuxReady = true;

	shared_ptr<SJtStartStreamReq> startReq = make_shared<SJtStartStreamReq>();
	startReq->m_terminalPhone = m_terminalId;
	startReq->m_ip = m_ip;
	startReq->m_tcpPort = m_port;
	startReq->m_udpPort = m_port;
	startReq->m_channel = m_channelId;
	startReq->m_streamType = m_streamType;
	std::future<int> fut = startReq->m_promise.get_future();
	m_server->StartLiveStream(startReq);

	char sdp[2048] = {0};
	int offset = 0;

	offset += snprintf(sdp + offset, sizeof(sdp) - offset,
	                   "v=0\r\n"
	                   "o=- 0 0 IN IP4 127.0.0.1\r\n"
	                   "s=JT1078\r\n"
	                   "c=IN IP4 127.0.0.1\r\n"
	                   "t=0 0\r\n");

	if (m_videoPt != 0) {
		offset += snprintf(sdp + offset, sizeof(sdp) - offset,
		                   "m=video 0 RTP/AVP %d\r\n"
		                   "a=rtpmap:%d %s/90000\r\n",
		                   m_videoPt, m_videoPt, m_videoCodec.c_str());
		if (m_videoCodec == "H264") {
			offset += snprintf(sdp + offset, sizeof(sdp) - offset,
			                   "a=fmtp:%d packetization-mode=1;\r\n", m_videoPt);
		}
	}

	if (m_audioPt != 0) {
		offset += snprintf(sdp + offset, sizeof(sdp) - offset,
		                   "m=audio 0 RTP/AVP %d\r\n"
		                   "a=rtpmap:%d %s/%d/%d\r\n"
		                   "a=fmtp:%d profile-level-id=1;"
		                   "mode=AAC-hbr;sizelength=13;indexlength=3;"
		                   "indexdeltalength=3;\r\n",
		                   m_audioPt, m_audioPt, "MPEG4-GENERIC", (int)(m_audioClock * 1000),
		                   m_audioChannels, m_audioPt);
	}

	m_sdp = sdp;
	m_demuxThread = make_unique<std::thread>(&MsJtSource::DemuxThread, this);

	std::thread waitWorker([this, fut = std::move(fut)]() mutable {
		int ret = fut.get();
		if (ret != 0) {
			MS_LOG_ERROR("JT source:%s failed to start live stream on terminal",
			             m_terminalId.c_str());
			this->SourceActiveClose();
		} else {
			MS_LOG_INFO("JT source:%s live stream started on terminal successfully",
			            m_terminalId.c_str());
		}
	});
	waitWorker.detach();
}

int MsJtSource::ReadBuffer(uint8_t *buf, int buf_size) {
	std::unique_lock<std::mutex> lock(m_queMtx);
	m_condVar.wait(lock, [this]() { return m_rtpDataQue.size() > 0 || m_isClosing.load(); });

	if (m_isClosing.load()) {
		return AVERROR_EOF;
	}

	if (m_rtpDataQue.size() <= 0) {
		return AVERROR(EAGAIN);
	}

	SData &sd = m_rtpDataQue.front();
	int toRead = buf_size;
	if (toRead > sd.m_len) {
		toRead = sd.m_len;
	}

	memcpy(buf, sd.m_uBuf.get(), toRead);
	if (toRead < sd.m_len) {
		memmove(sd.m_uBuf.get(), sd.m_uBuf.get() + toRead, sd.m_len - toRead);
	}
	sd.m_len -= toRead;

	if (sd.m_len == 0) {
		m_recyleQue.emplace(std::move(sd));
		m_rtpDataQue.pop();
	}

	return toRead;
}

void MsJtSource::DemuxThread() {
	int ret;
	const int rtp_buff_size = 4096;
	AVPacket *pkt = nullptr;
	bool fisrtVideoPkt = true;
	AVFormatContext *fmtCtx = nullptr;
	AVIOContext *sdpAvioCtx = nullptr;
	AVIOContext *rtpAvioCtx = nullptr;

	sdpAvioCtx = avio_alloc_context(
	    static_cast<unsigned char *>(av_malloc(m_sdp.size())), m_sdp.size(), 0, this,
	    [](void *opaque, uint8_t *buf, int buf_size) -> int {
		    auto source = static_cast<MsJtSource *>(opaque);
		    if (source->m_readSdpPos >= (int)source->m_sdp.size()) {
			    return AVERROR_EOF;
		    }

		    int left = source->m_sdp.size() - source->m_readSdpPos;
		    if (buf_size > left) {
			    buf_size = left;
		    }

		    memcpy(buf, source->m_sdp.c_str() + source->m_readSdpPos, buf_size);
		    source->m_readSdpPos += buf_size;
		    return buf_size;
	    },
	    NULL, NULL);

	rtpAvioCtx = avio_alloc_context(
	    static_cast<unsigned char *>(av_malloc(rtp_buff_size)), rtp_buff_size, 1, this,
	    [](void *opaque, uint8_t *buf, int buf_size) -> int {
		    auto source = static_cast<MsJtSource *>(opaque);
		    return source->ReadBuffer(buf, buf_size);
	    },
	    [](void *, IO_WRITE_BUF_TYPE *, int buf_size) -> int { return buf_size; }, NULL);

	fmtCtx = avformat_alloc_context();
	fmtCtx->pb = sdpAvioCtx;

	AVDictionary *options = nullptr;
	av_dict_set(&options, "sdp_flags", "custom_io", 0);
	av_dict_set_int(&options, "reorder_queue_size", 0, 0);
	av_dict_set(&options, "protocol_whitelist", "file,rtp,udp", 0);

	ret = avformat_open_input(&fmtCtx, "", nullptr, &options);
	av_dict_free(&options);

	if (ret != 0) {
		MS_LOG_ERROR("JT source:%s avformat_open_input failed:%d", m_streamID.c_str(), ret);
		goto err;
	}

	av_freep(&sdpAvioCtx->buffer);
	avio_context_free(&sdpAvioCtx);
	sdpAvioCtx = nullptr;

	fmtCtx->pb = rtpAvioCtx;

	if ((ret = avformat_find_stream_info(fmtCtx, nullptr)) != 0) {
		MS_LOG_ERROR("JT source:%s avformat_find_stream_info failed:%d", m_streamID.c_str(), ret);
		goto err;
	}

	ret = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
	if (ret < 0) {
		MS_LOG_ERROR("JT source:%s no video stream found", m_streamID.c_str());
		goto err;
	}
	m_videoIdx = ret;
	m_video = fmtCtx->streams[m_videoIdx];

	ret = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
	if (ret >= 0) {
		AVCodecParameters *codecPar = fmtCtx->streams[ret]->codecpar;
		if (codecPar->codec_id == AV_CODEC_ID_AAC) {
			m_audioIdx = ret;
			m_audio = fmtCtx->streams[m_audioIdx];
		}
	}

	this->NotifyStreamInfo();

	pkt = av_packet_alloc();

	while (!m_isClosing.load() && av_read_frame(fmtCtx, pkt) >= 0) {
		if (pkt->stream_index == m_videoIdx || pkt->stream_index == m_audioIdx) {
			if (pkt->stream_index == m_videoIdx) {
				if (fisrtVideoPkt) {
					fisrtVideoPkt = false;
					// TODO: quick fix for some RTSP stream with first pkt
					// pts=dts=AV_NOPTS_VALUE
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
	if (!m_isClosing.load())
		this->SourceActiveClose();
}

void MsJtSource::SourceReleaseRes() {
	if (m_demuxThread) {
		m_isClosing.store(true);
		m_condVar.notify_all();
		if (m_demuxThread->joinable())
			m_demuxThread->join();
		m_demuxThread = nullptr;
	}

	while (m_rtpDataQue.size() > 0) {
		m_rtpDataQue.pop();
	}

	while (m_recyleQue.size() > 0) {
		m_recyleQue.pop();
	}

	if (m_acceptEvent) {
		m_server->DelEvent(m_acceptEvent);
		m_acceptEvent = nullptr;
	}

	if (m_tcpEvent) {
		m_server->DelEvent(m_tcpEvent);
		m_tcpEvent = nullptr;
	}

	if (m_udpEvent) {
		m_server->DelEvent(m_udpEvent);
		m_udpEvent = nullptr;
	}

	if (m_videoPt > 0) {
		m_server->StopLiveStream(m_terminalId, m_channelId);
		m_videoPt = 0;
	}
}
