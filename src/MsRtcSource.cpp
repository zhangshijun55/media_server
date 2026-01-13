#include "MsRtcSource.h"
#include "MsResManager.h"
#include <sstream>

MsRtcSource::~MsRtcSource() {
	MS_LOG_INFO("~MsRtcSource %s", _sessionId.c_str());
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

void MsRtcSource::AddSink(std::shared_ptr<MsMediaSink> sink) {
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

void MsRtcSource::NotifyStreamPacket(AVPacket *pkt) {
	std::lock_guard<std::mutex> lock(m_sinkMutex);
	for (auto &sink : m_sinks) {
		if (sink) {
			sink->OnStreamPacket(pkt);
		}
	}
}

void MsRtcSource::SourceActiveClose() {
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

void MsRtcSource::GenerateSdp() {
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

void MsRtcSource::StartRtpDemux() {
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
		    auto rpc = static_cast<MsRtcSource *>(opaque);
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
		    auto rpc = static_cast<MsRtcSource *>(opaque);
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

void MsRtcSource::WriteBuffer(const void *buf, int size) {
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

int MsRtcSource::ReadBuffer(uint8_t *buf, int buf_size) {
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
