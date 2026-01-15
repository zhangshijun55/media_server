#include "MsRtcSink.h"
#include "MsCommon.h"
#include "MsConfig.h"
#include "MsHttpMsg.h"
#include "MsLog.h"
#include "MsMsg.h"
#include "MsMsgDef.h"
#include "MsReactor.h"
#include "MsThreadPool.h"

void MsRtcSink::ReleaseResources() {
	if (m_videoFmtCtx) {
		if (m_videoFmtCtx->pb) {
			av_freep(&m_videoFmtCtx->pb->buffer);
			avio_context_free(&m_videoFmtCtx->pb);
		}
		avformat_free_context(m_videoFmtCtx);
		m_videoFmtCtx = nullptr;
	}

	if (m_audioFmtCtx) {
		if (m_audioFmtCtx->pb) {
			av_freep(&m_audioFmtCtx->pb->buffer);
			avio_context_free(&m_audioFmtCtx->pb);
		}
		avformat_free_context(m_audioFmtCtx);
		m_audioFmtCtx = nullptr;
	}

	if (_pc) {
		_pc->close();
		_pc.reset();
	}

	_videoTrack.reset();
	_audioTrack.reset();

	while (m_queAudioPkts.size() > 0) {
		AVPacket *pkt = m_queAudioPkts.front();
		av_packet_free(&pkt);
		m_queAudioPkts.pop();
	}

	if (_onWhepPeerClosed) {

		_onWhepPeerClosed(_sessionId);
		_onWhepPeerClosed = nullptr;
	}
}

void MsRtcSink::SinkActiveClose() {
	m_error = true;

	this->DetachSource();
	this->ReleaseResources();
}

void MsRtcSink::OnSourceClose() {
	m_error = true;
	this->ReleaseResources();
}

void MsRtcSink::SetupWebRTC(const string &offerSdp, const string &httpVersion,
                            const string &location,
                            std::function<void(const string &)> onWhepPeerClosed) {
	_offerSdp = offerSdp;
	_httpVersion = httpVersion;
	_location = location;
	_onWhepPeerClosed = onWhepPeerClosed;
}

int MsRtcSink::CreateTracksAndAnswer() {
	if (m_error) {
		MS_LOG_WARN("whep:%s cannot create tracks, sink in error state", _sessionId.c_str());
		return -1;
	}

	rtc::Configuration config;
	config.enableIceTcp = true;
	config.enableIceUdpMux = true;
	config.portRangeBegin = MsConfig::Instance()->GetConfigInt("rtcPort");
	config.portRangeEnd = config.portRangeBegin;

	_pc = make_shared<rtc::PeerConnection>(config);

	rtc::Description offer(_offerSdp, "offer");
	for (int i = 0; i < offer.mediaCount(); ++i) {
		auto var = offer.media(i);
		auto media = std::get_if<rtc::Description::Media *>(&var);
		if (media == nullptr) {
			continue;
		}
		auto pMedia = *media;
		string type = pMedia->type();

		auto payloadTypes = pMedia->payloadTypes(); // Preload payload types
		for (auto &pt : payloadTypes) {
			rtc::Description::Media::RtpMap *rtpMap = nullptr;
			try {
				rtpMap = pMedia->rtpMap(pt);
			} catch (...) {
				continue;
			}

			MS_LOG_INFO("whep:%s offer media:%s pt:%d codec:%s", _sessionId.c_str(), type.c_str(),
			            pt, rtpMap->format.c_str());
			if ((rtpMap->format == "OPUS" || rtpMap->format == "opus") && m_audio && !_audioTrack) {
				if (m_audio->codecpar->codec_id == AV_CODEC_ID_OPUS) {
					_audioPt = pt;
					_audioCodec = rtpMap->format;

					rtc::Description::Audio audioDesc(pMedia->mid());
					audioDesc.addOpusCodec(pt);
					auto ssrcs = pMedia->getSSRCs();
					for (auto ssrc : ssrcs) {
						audioDesc.addSSRC(ssrc, pMedia->getCNameForSsrc(ssrc));
					}
					_audioTrack = _pc->addTrack(std::move(audioDesc));
				} else if (m_audio->codecpar->codec_id == AV_CODEC_ID_AAC) {
					// do not support AAC in whep for now
				}
			} else if (rtpMap->format == "H264" && !_videoTrack) {
				if (m_video->codecpar->codec_id == AV_CODEC_ID_H264) {
					_videoPt = pt;
					_videoCodec = rtpMap->format;

					rtc::Description::Video videoDesc(pMedia->mid());
					videoDesc.addH264Codec(pt);
					auto ssrcs = pMedia->getSSRCs();
					for (auto ssrc : ssrcs) {
						videoDesc.addSSRC(ssrc, pMedia->getCNameForSsrc(ssrc));
					}
					_videoTrack = _pc->addTrack(std::move(videoDesc));
				}
			} else if (rtpMap->format == "H265" && !_videoTrack) {
				if (m_video->codecpar->codec_id == AV_CODEC_ID_H265) {
					_videoPt = pt;
					_videoCodec = rtpMap->format;

					rtc::Description::Video videoDesc(pMedia->mid());
					videoDesc.addH265Codec(pt);
					auto ssrcs = pMedia->getSSRCs();
					for (auto ssrc : ssrcs) {
						videoDesc.addSSRC(ssrc, pMedia->getCNameForSsrc(ssrc));
					}
					_videoTrack = _pc->addTrack(std::move(videoDesc));
				}
			}
		}
	}

	// Setup state change callback
	string sessionId = _sessionId;
	auto onPeerClosed = _onWhepPeerClosed;
	_pc->onStateChange([sessionId, onPeerClosed](rtc::PeerConnection::State state) {
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
		case rtc::PeerConnection::State::Closed:
			strState = "Closed";
			if (onPeerClosed) {
				onPeerClosed(sessionId);
			}
			break;
		}
		MS_LOG_INFO("whep:%s state:%s", sessionId.c_str(), strState.c_str());
	});

	_pc->onTrack([this, sessionId](shared_ptr<rtc::Track> track) {
		weak_ptr<rtc::Track> weak_track = track;
		track->onOpen([weak_track, sessionId]() {
			auto track = weak_track.lock();
			if (track)
				MS_LOG_INFO("pc:%s track open:%s", sessionId.c_str(), track->mid().c_str());
		});

		std::shared_ptr<rtc::RtcpReceivingSession> session =
		    std::make_shared<rtc::RtcpReceivingSession>();
		track->setMediaHandler(session);
	});

	_pc->onLocalDescription([](rtc::Description description) {});

	std::weak_ptr<rtc::PeerConnection> weak_pc = _pc;
	std::weak_ptr<MsSocket> weak_sock = _sock;
	string version = _httpVersion;
	string location = _location;

	_pc->onGatheringStateChange([this, weak_pc, weak_sock, sessionId, version,
	                             location](rtc::PeerConnection::GatheringState state) {
		if (state == rtc::PeerConnection::GatheringState::Complete) {
			auto pc = weak_pc.lock();
			if (!pc)
				return;

			auto sock = weak_sock.lock();
			if (!sock)
				return;

			MsThreadPool::Instance().enqueue([this]() { this->StartRtpMux(); });

			auto description = pc->localDescription();
			string sdp = description->generateSdp();

			MsHttpMsg rsp;
			rsp.m_version = version;

			if (_videoPt == 0 && _audioPt == 0) {
				MS_LOG_ERROR("whep:%s no valid tracks to create answer", sessionId.c_str());
				rsp.m_status = "500";
				rsp.m_reason = "Internal Server Error";
			} else {
				rsp.m_status = "201";
				rsp.m_reason = "Created";
				rsp.m_location.SetValue(location);
				rsp.m_contentType.SetValue("application/sdp");
				rsp.m_exposeHeader.SetValue("Location");
				rsp.SetBody(sdp.c_str(), sdp.size());
			}

			SendHttpRspEx(sock.get(), rsp);

			MS_LOG_INFO("whep:%s SDP answer sent", sessionId.c_str());

			// should send MS_RTC_DEL_SOCK message to MsRtcServer to delete socket
			MsMsg msg;
			msg.m_msgID = MS_RTC_DEL_SOCK;
			msg.m_strVal = sessionId;
			msg.m_dstType = MS_RTC_SERVER;
			msg.m_dstID = 1;
			MsReactorMgr::Instance()->PostMsg(msg);
		}
	});

	// Parse and set remote description
	try {
		_pc->setRemoteDescription(std::move(offer));
	} catch (const std::exception &e) {
		MS_LOG_ERROR("whep:%s setRemoteDescription exception:%s", _sessionId.c_str(), e.what());
		m_error = true;

		MsHttpMsg rsp;
		rsp.m_version = _httpVersion;
		rsp.m_status = "500";
		rsp.m_reason = "Internal Server Error";
		if (_sock) {
			SendHttpRspEx(_sock.get(), rsp);
		}

		return -1;
	}

	return 0;
}

void MsRtcSink::StartRtpMux() {
	if (m_streamReady) {
		MS_LOG_INFO("whep:%s RTP muxing thread already started", _sessionId.c_str());
		return;
	}

	if (m_error) {
		MS_LOG_WARN("whep:%s cannot start muxing thread, sink in error state", _sessionId.c_str());
		this->SinkActiveClose();
		return;
	}

	if (_videoPt == 0 && _audioPt == 0) {
		MS_LOG_WARN("whep:%s no valid tracks to mux, sink in error state", _sessionId.c_str());
		this->SinkActiveClose();
		return;
	}

	int buf_size = 2048;
	int ret;
	AVDictionary *opts = nullptr;
	char ptStr[8];

	// Setup video RTP muxer
	m_videoPb = avio_alloc_context(
	    static_cast<unsigned char *>(av_malloc(buf_size)), buf_size, 1, this, nullptr,
	    [](void *opaque, IO_WRITE_BUF_TYPE *buf, int buf_size) -> int {
		    MsRtcSink *sink = static_cast<MsRtcSink *>(opaque);
		    return sink->WriteBuffer(buf, buf_size, 1);
	    },
	    nullptr);
	m_videoPb->max_packet_size = 1200; // MTU for WebRTC

	avformat_alloc_output_context2(&m_videoFmtCtx, nullptr, "rtp", nullptr);
	if (!m_videoFmtCtx || !m_videoPb) {
		MS_LOG_ERROR("Failed to allocate video format context or IO context");
		goto err;
	}

	m_videoFmtCtx->pb = m_videoPb;
	m_videoFmtCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

	m_outVideo = avformat_new_stream(m_videoFmtCtx, NULL);
	ret = avcodec_parameters_copy(m_outVideo->codecpar, m_video->codecpar);
	if (ret < 0) {
		MS_LOG_ERROR("Failed to copy video codec parameters");
		goto err;
	}
	m_outVideo->codecpar->codec_tag = 0;

	// Set RTP payload type for video
	snprintf(ptStr, sizeof(ptStr), "%d", _videoPt);
	av_dict_set(&opts, "payload_type", ptStr, 0);
	av_dict_set(&opts, "rtpflags", "skip_rtcp", 0);
	ret = avformat_write_header(m_videoFmtCtx, &opts);
	av_dict_free(&opts);
	if (ret < 0) {
		MS_LOG_ERROR("Error writing video RTP header");
		goto err;
	}

	// Setup audio RTP muxer if audio exists
	if (m_audio && _audioPt != 0) {
		m_audioPb = avio_alloc_context(
		    static_cast<unsigned char *>(av_malloc(buf_size)), buf_size, 1, this, nullptr,
		    [](void *opaque, IO_WRITE_BUF_TYPE *buf, int buf_size) -> int {
			    MsRtcSink *sink = static_cast<MsRtcSink *>(opaque);
			    return sink->WriteBuffer(buf, buf_size, 0);
		    },
		    nullptr);
		m_audioPb->max_packet_size = 1200;

		avformat_alloc_output_context2(&m_audioFmtCtx, nullptr, "rtp", nullptr);
		if (!m_audioFmtCtx || !m_audioPb) {
			MS_LOG_ERROR("Failed to allocate audio format context or IO context");
			goto err;
		}

		m_audioFmtCtx->pb = m_audioPb;
		m_audioFmtCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

		m_outAudio = avformat_new_stream(m_audioFmtCtx, NULL);
		ret = avcodec_parameters_copy(m_outAudio->codecpar, m_audio->codecpar);
		if (ret < 0) {
			MS_LOG_ERROR("Failed to copy audio codec parameters");
			goto err;
		}
		m_outAudio->codecpar->codec_tag = 0;

		AVCodecParameters *codecpar = m_outAudio->codecpar;
		// Generate AAC extradata if missing
		if (codecpar->extradata_size == 0 && codecpar->sample_rate > 0 &&
		    codecpar->ch_layout.nb_channels > 0 && codecpar->ch_layout.nb_channels < 3 &&
		    codecpar->codec_id == AV_CODEC_ID_AAC) {
			int samplingFrequencyIndex = 15;
			const static int sampleRateTable[] = {96000, 88200, 64000, 48000, 44100, 32000, 24000,
			                                      22050, 16000, 12000, 11025, 8000,  7350};
			for (int i = 0; i < 13; i++) {
				if (sampleRateTable[i] == codecpar->sample_rate) {
					samplingFrequencyIndex = i;
					break;
				}
			}

			int audioObjectType = (codecpar->profile > 0) ? codecpar->profile + 1 : 2;
			int channelConfiguration = codecpar->ch_layout.nb_channels;

			uint16_t config = 0;
			config |= (audioObjectType & 0x1F) << 11;
			config |= (samplingFrequencyIndex & 0x0F) << 7;
			config |= (channelConfiguration & 0x0F) << 3;

			uint8_t extradata[2];
			extradata[0] = (config >> 8) & 0xFF;
			extradata[1] = config & 0xFF;

			codecpar->extradata = (uint8_t *)av_mallocz(2 + AV_INPUT_BUFFER_PADDING_SIZE);
			memcpy(codecpar->extradata, extradata, 2);
			codecpar->extradata_size = 2;
		}

		// Set RTP payload type for audio
		snprintf(ptStr, sizeof(ptStr), "%d", _audioPt);
		av_dict_set(&opts, "payload_type", ptStr, 0);
		av_dict_set(&opts, "rtpflags", "skip_rtcp", 0);
		ret = avformat_write_header(m_audioFmtCtx, &opts);
		av_dict_free(&opts);
		if (ret < 0) {
			MS_LOG_ERROR("Error writing audio RTP header");
			goto err;
		}
	}

	m_streamReady = true;
	MS_LOG_INFO("MsRtcSink %s stream ready, video:%s audio:%s", _sessionId.c_str(),
	            _videoCodec.c_str(), _audioCodec.c_str());
	return;

err:
	this->SinkActiveClose();
}

void MsRtcSink::OnStreamInfo(AVStream *video, int videoIdx, AVStream *audio, int audioIdx) {
	if (m_error || !video)
		return;

	MsMediaSink::OnStreamInfo(video, videoIdx, audio, audioIdx);

	// Now create WebRTC tracks based on actual stream info and send SDP answer
	if (CreateTracksAndAnswer() < 0) {
		goto err;
	}

	return;

err:
	m_error = true;
	this->DetachSourceNoLock();
	this->ReleaseResources();
}

void MsRtcSink::OnStreamPacket(AVPacket *pkt) {
	if (!m_streamReady || m_error) {
		return;
	}

	int ret;
	bool isVideo = pkt->stream_index == m_videoIdx;
	AVFormatContext *fmtCtx = isVideo ? m_videoFmtCtx : m_audioFmtCtx;
	AVStream *outSt = isVideo ? m_outVideo : m_outAudio;
	AVStream *inSt = isVideo ? m_video : m_audio;
	int outIdx = isVideo ? m_outVideoIdx : m_outAudioIdx;
	int inIdx = pkt->stream_index;
	int64_t orig_pts = pkt->pts;
	int64_t orig_dts = pkt->dts;

	if (!fmtCtx) {
		return;
	}

	// Drop non-key video frame at the beginning
	if (isVideo && m_firstVideo) {
		if (!(pkt->flags & AV_PKT_FLAG_KEY)) {
			return;
		}
	} else {
		// buffer audio pkts
		if (m_firstVideo) {
			AVPacket *apkt = av_packet_clone(pkt);
			m_queAudioPkts.push(apkt);
			return;
		}
	}

	av_packet_rescale_ts(pkt, inSt->time_base, outSt->time_base);
	pkt->pos = -1;
	pkt->stream_index = outIdx;

	if (pkt->pts == AV_NOPTS_VALUE) {
		MS_LOG_WARN("pkt pts is AV_NOPTS_VALUE, streamID:%s, sinkID:%d codec:%d "
		            "pts:%ld dts:%ld size:%d",
		            m_streamID.c_str(), m_sinkID, inSt->codecpar->codec_id, pkt->pts, pkt->dts,
		            pkt->size);
		MS_LOG_INFO("ori pts:%ld dts:%ld, in timebase %d/%d, out timebase %d/%d", orig_pts,
		            orig_dts, inSt->time_base.num, inSt->time_base.den, outSt->time_base.num,
		            outSt->time_base.den);
	}

	if (isVideo) {
		if (m_firstVideo) {
			m_firstVideo = false;
			m_firstVideoPts = pkt->pts;
			m_firstVideoDts = pkt->dts;
		}
		pkt->pts -= m_firstVideoPts;
		if (pkt->dts != AV_NOPTS_VALUE && m_firstVideoDts != AV_NOPTS_VALUE) {
			pkt->dts -= m_firstVideoDts;
		}
	} else {
		if (m_firstAudio) {
			m_firstAudio = false;
			m_firstAudioPts = pkt->pts;
			m_firstAudioDts = pkt->dts;
		}
		pkt->pts -= m_firstAudioPts;
		if (pkt->dts != AV_NOPTS_VALUE && m_firstAudioDts != AV_NOPTS_VALUE) {
			pkt->dts -= m_firstAudioDts;
		}
	}

	ret = av_write_frame(fmtCtx, pkt);
	if (ret < 0) {
		char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
		av_strerror(ret, errbuf, sizeof(errbuf));
		MS_LOG_ERROR("Error writing frame to RTC sink, ret:%d %s", ret, errbuf);
	}

	while (m_queAudioPkts.size() && inIdx == m_videoIdx) {
		AVPacket *apkt = m_queAudioPkts.front();
		m_queAudioPkts.pop();
		int64_t ori_ms = orig_pts * 1000L * inSt->time_base.num / inSt->time_base.den;
		int64_t apkt_ms = apkt->pts * 1000L * m_audio->time_base.num / m_audio->time_base.den;
		int64_t diff = abs(ori_ms - apkt_ms);

		if (diff < 131) { // allow max 131ms diff
			// send pkt
			this->OnStreamPacket(apkt);
		} else {
			// drop pkt
			MS_LOG_WARN("StreamID:%s, sinkID:%d drop buffered audio pkt, ori_ms:%lld "
			            "apkt_ms:%lld diff:%lld",
			            m_streamID.c_str(), m_sinkID, ori_ms, apkt_ms, diff);
		}
		av_packet_free(&apkt);
	}

	// Restore original values
	pkt->pts = orig_pts;
	pkt->dts = orig_dts;
	pkt->pos = -1;
	pkt->stream_index = inIdx;
}

int MsRtcSink::WriteBuffer(const uint8_t *buf, int buf_size, int8_t isVideo) {
	if (m_error || buf_size < 12) {
		return -1;
	}

	// Send RTP packet via WebRTC track
	auto track = isVideo ? _videoTrack : _audioTrack;
	if (track && track->isOpen()) {
		try {
			rtc::binary data(reinterpret_cast<const std::byte *>(buf),
			                 reinterpret_cast<const std::byte *>(buf + buf_size));
			track->send(data);
		} catch (const std::exception &e) {
			MS_LOG_WARN("Failed to send RTP packet: %s", e.what());
			return -1;
		}
	}

	return buf_size;
}
