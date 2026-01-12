#include "MsHttpStream.h"
#include "MsConfig.h"
#include "MsDevMgr.h"
#include "MsLog.h"
#include "MsMsgDef.h"
#include "MsPortAllocator.h"
#include "MsResManager.h"
#include "MsSourceFactory.h"
#include "nlohmann/json.hpp"
#include <thread>

using json = nlohmann::json;

class MsHttpSink : public MsMediaSink, public MsEventHandler {
public:
	MsHttpSink(const std::string &type, const std::string &streamID, int sinkID,
	           std::shared_ptr<MsSocket> sock)
	    : MsMediaSink(type, streamID, sinkID), m_sock(sock) {}

	void HandleRead(shared_ptr<MsEvent> evt) override {
		char buf[512];
		int ret = m_sock->Recv(buf, 512);
		if (ret == 0) {
			this->SinkActiveClose();
		}
	}

	void HandleClose(shared_ptr<MsEvent> evt) override {
		MS_LOG_ERROR("http sink socket closed, streamID:%s, sinkID:%d", m_streamID.c_str(),
		             m_sinkID);
		this->SinkActiveClose();
	}

	void HandleWrite(shared_ptr<MsEvent> evt) override {
		if (m_error)
			return;

		int ret = 0;
		auto sock = evt->GetSharedSocket();

		while (m_queData.size()) {
			SData &sd = m_queData.front();
			uint8_t *pBuf = sd.m_buf;

			while (sd.m_len > 0) {
				int psend = 0;
				ret = sock->Send((const char *)pBuf, sd.m_len, &psend);
				if (psend > 0) {
					sd.m_len -= psend;
					pBuf += psend;
				}

				if (ret >= 0) {
					// do nothing, handled in psend
				} else if (ret == MS_TRY_AGAIN) {
					if (pBuf != sd.m_buf) {
						memmove(sd.m_buf, pBuf, sd.m_len);
					}
					return;
				} else {
					MS_LOG_INFO("http sink streamID:%s, sinkID:%d err:%d", m_streamID.c_str(),
					            m_sinkID, MS_LAST_ERROR);
					this->SinkActiveClose();
					return;
				}
			}

			delete[] sd.m_buf;
			std::lock_guard<std::mutex> lock(m_queDataMutex);
			m_queData.pop();
		}

		// unregist write
		m_evt->SetEvent(MS_FD_READ | MS_FD_CLOSE);
		m_reactor->ModEvent(m_evt);

		MS_LOG_INFO("http sink streamID:%s, sinkID:%d all data sent", m_streamID.c_str(), m_sinkID);
	}

	// TODO: add proper time duration handling
	// TODO: gb record play back, ts has aac timestamp gap issue when
	//       the audio pkt has dup timestamp, flv muxer can handle it,
	//       but ts muxer not, need further investigate
	void OnStreamInfo(AVStream *video, int videoIdx, AVStream *audio, int audioIdx) override {
		if (m_error || !video)
			return;
		MsMediaSink::OnStreamInfo(video, videoIdx, audio, audioIdx);
		int buf_size = 2048;
		int ret;

		m_reactor = MsReactorMgr::Instance()->GetReactor(MS_COMMON_REACTOR, 1);
		if (!m_reactor) {
			MS_LOG_ERROR("reactor not found for streamID:%s", m_streamID.c_str());
			goto err;
		}

		m_sock->SetNonBlock();
		m_evt = std::make_shared<MsEvent>(m_sock, MS_FD_READ | MS_FD_CLOSE, shared_from_this());
		m_reactor->AddEvent(m_evt);

		m_pb = avio_alloc_context(
		    static_cast<unsigned char *>(av_malloc(buf_size)), buf_size, 1, this, nullptr,
		    [](void *opaque, const uint8_t *buf, int buf_size) -> int {
			    MsHttpSink *sink = static_cast<MsHttpSink *>(opaque);
			    return sink->WriteBuffer(buf, buf_size);
		    },
		    nullptr);

		if (m_type == "flv")
			avformat_alloc_output_context2(&m_fmtCtx, nullptr, "flv", nullptr);
		else if (m_type == "ts")
			avformat_alloc_output_context2(&m_fmtCtx, nullptr, "mpegts", nullptr);
		else
			avformat_alloc_output_context2(&m_fmtCtx, nullptr, m_type.c_str(), nullptr);
		if (!m_fmtCtx || !m_pb) {
			MS_LOG_ERROR("Failed to allocate format context or IO context");
			goto err;
		}

		m_fmtCtx->pb = m_pb;
		m_fmtCtx->flags |= AVFMT_FLAG_CUSTOM_IO;

		m_outVideo = avformat_new_stream(m_fmtCtx, NULL);
		ret = avcodec_parameters_copy(m_outVideo->codecpar, m_video->codecpar);
		if (ret < 0) {
			MS_LOG_ERROR("Failed to copy codec parameters to output stream");
			goto err;
		}
		m_outVideo->codecpar->codec_tag = 0;

		if (m_audio) {
			m_outAudio = avformat_new_stream(m_fmtCtx, NULL);
			ret = avcodec_parameters_copy(m_outAudio->codecpar, m_audio->codecpar);
			if (ret < 0) {
				MS_LOG_ERROR("Failed to copy codec parameters to output stream");
				goto err;
			}
			m_outAudio->codecpar->codec_tag = 0;
		}

		ret = avformat_write_header(m_fmtCtx, NULL);
		if (ret < 0) {
			MS_LOG_ERROR("Error occurred when writing header to HTTP sink");
			goto err;
		}

		m_streamReady = true;
		return;

	err:
		m_error = true;
		this->DetachSourceNoLock();
		this->ReleaseResources();
	}

	int WriteBuffer(const uint8_t *buf, int buf_size) {
		if (m_error)
			return -1;

		if (m_firstPacket) {
			m_firstPacket = false;
			MsHttpMsg rsp;

			rsp.m_version = "HTTP/1.1";
			rsp.m_status = "200";
			rsp.m_reason = "OK";
			rsp.m_connection.SetValue("close");
			if (m_type == "flv") {
				rsp.m_contentType.SetValue("video/x-flv");
			} else if (m_type == "ts") {
				rsp.m_contentType.SetValue("video/MP2T");
			} else {
				rsp.m_contentType.SetValue("application/octet-stream");
			}

			rsp.m_transport.SetValue("chunked");
			rsp.m_allowOrigin.SetValue("*");
			rsp.m_allowMethod.SetValue("GET, POST, OPTIONS");
			rsp.m_allowHeader.SetValue(
			    "DNT,X-Mx-ReqToken,range,Keep-Alive,User-Agent,X-Requested-With,If-"
			    "Modified-Since,Cache-Control,Content-Type,Authorization");

			SendHttpRsp(m_sock.get(), rsp);
		}

		// Format chunk size in hex
		char chunk_header[32];
		int header_len = snprintf(chunk_header, sizeof(chunk_header), "%x\r\n", buf_size);
		int total_chunk_size = header_len + buf_size + 2; // +2 for \r\n after data

		if (m_queData.size()) {
			SData sd;
			sd.m_buf = new uint8_t[total_chunk_size];
			memcpy(sd.m_buf, chunk_header, header_len);
			memcpy(sd.m_buf + header_len, buf, buf_size);
			memcpy(sd.m_buf + header_len + buf_size, "\r\n", 2);
			sd.m_len = total_chunk_size;
			std::lock_guard<std::mutex> lock(m_queDataMutex);
			m_queData.push(sd);
		} else {
			if (m_sock->BlockSend((const char *)chunk_header, header_len) < 0) {
				MS_LOG_ERROR("http sink send chunk header failed");
				this->PassiveClose();
				return buf_size;
			}

			const char *pBuf = (const char *)buf;
			int pLen = buf_size;
			int ret = 0;

			while (pLen > 0) {
				int psend = 0;
				m_sock->Send(pBuf, pLen, &psend);
				if (psend > 0) {
					pLen -= psend;
					pBuf += psend;
				}

				if (ret >= 0) {
					// do nothing, handled in psend
				} else if (ret == MS_TRY_AGAIN) {
					SData sd;
					sd.m_buf = new uint8_t[pLen + 2];
					memcpy(sd.m_buf, pBuf, pLen);
					memcpy(sd.m_buf + pLen, "\r\n", 2);
					sd.m_len = pLen + 2;
					{
						std::lock_guard<std::mutex> lock(m_queDataMutex);
						m_queData.push(sd);
					}

					// regist write
					m_evt->SetEvent(MS_FD_READ | MS_FD_CLOSE | MS_FD_CONNECT);
					m_reactor->ModEvent(m_evt);

					MS_LOG_INFO("sink %s:%d regist write", m_type.c_str(), m_sinkID);

					return buf_size;
				} else {
					MS_LOG_INFO("sink %s:%d err:%d", m_type.c_str(), m_sinkID, MS_LAST_ERROR);
					this->PassiveClose();
					return buf_size;
				}
			}

			if (m_sock->BlockSend("\r\n", 2) < 0) {
				MS_LOG_ERROR("http sink send chunk tail failed");
				this->PassiveClose();
				return buf_size;
			}
		}

		return buf_size;
	}

	void SinkActiveClose() {
		m_error = true;

		this->DetachSource();
		this->ReleaseResources();
	}

	// TODO: need fix
	void PassiveClose() {
		m_error = true;
		// this->DetachSourceNoLock();
		// this->ReleaseResources();
	}

	void ReleaseResources() {
		this->clear_que();

		if (m_evt && m_reactor) {
			m_reactor->DelEvent(m_evt);
			m_evt = nullptr;
			m_reactor = nullptr;
		}

		if (m_fmtCtx) {
			av_write_trailer(m_fmtCtx);
			if (m_fmtCtx->pb) {
				avio_flush(m_fmtCtx->pb);
				av_freep(&m_fmtCtx->pb->buffer);
				avio_context_free(&m_fmtCtx->pb);
			}
			avformat_free_context(m_fmtCtx);
			m_fmtCtx = nullptr;
		}
	}

	void OnSourceClose() override {
		m_error = true;
		this->ReleaseResources();
	}

	void OnStreamPacket(AVPacket *pkt) override {
		if (!m_streamReady || m_error) {
			return;
		}

		int ret;
		AVStream *outSt = pkt->stream_index == m_videoIdx ? m_outVideo : m_outAudio;
		AVStream *inSt = pkt->stream_index == m_videoIdx ? m_video : m_audio;
		int outIdx = pkt->stream_index == m_videoIdx ? m_outVideoIdx : m_outAudioIdx;
		int inIdx = pkt->stream_index;
		int64_t orig_pts = pkt->pts;
		int64_t orig_dts = pkt->dts;

		// drop non-key video frame at the beginning, how about audio?
		if (inIdx == m_videoIdx) {
			if (m_firstVideo) {
				if (!(pkt->flags & AV_PKT_FLAG_KEY)) {
					return;
				}
			}
		} else {
			// buffer audio pkts
			if (m_firstVideo) {
				AVPacket *apkt = av_packet_clone(pkt);
				m_queAudioPkts.push(apkt);
				return;
			}
		}

		/* copy packet */
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

		if (inIdx == m_videoIdx) {
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

		ret = av_write_frame(m_fmtCtx, pkt);
		if (ret < 0) {
			MS_LOG_ERROR("Error writing frame to HTTP sink, ret:%d %s", ret, av_err2str(ret));
			MS_LOG_INFO("streamID:%s, sinkID:%d codec:%d pts:%ld dts:%ld size:%d",
			            m_streamID.c_str(), m_sinkID, inSt->codecpar->codec_id, pkt->pts, pkt->dts,
			            pkt->size);
			MS_LOG_INFO("ori pts:%ld dts:%ld, in timebase %d/%d, out timebase %d/%d", orig_pts,
			            orig_dts, inSt->time_base.num, inSt->time_base.den, outSt->time_base.num,
			            outSt->time_base.den);
			MS_LOG_INFO("streamID:%s, sinkID:%d  1st vpts:%ld vdts:%ld apts:%ld adts:%ld",
			            m_streamID.c_str(), m_sinkID, m_firstVideoPts, m_firstVideoDts,
			            m_firstAudioPts, m_firstAudioDts);
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
				MS_LOG_WARN("treamID:%s, sinkID:%d drop buffered audio pkt, ori_ms:%lld "
				            "apkt_ms:%lld diff:%lld",
				            m_streamID.c_str(), m_sinkID, ori_ms, apkt_ms, diff);
			}
			av_packet_free(&apkt);
		}

		pkt->pts = orig_pts;
		pkt->dts = orig_dts;
		pkt->pos = -1;
		pkt->stream_index = inIdx;
	}

private:
	void clear_que() {
		while (m_queData.size()) {
			SData &sd = m_queData.front();
			delete[] sd.m_buf;
			m_queData.pop();
		}
		while (m_queAudioPkts.size()) {
			AVPacket *pkt = m_queAudioPkts.front();
			m_queAudioPkts.pop();
			av_packet_free(&pkt);
		}
	}

	std::atomic_bool m_streamReady{false};
	bool m_error = false;
	bool m_firstVideo = true;
	bool m_firstAudio = true;
	int64_t m_firstVideoPts = 0;
	int64_t m_firstVideoDts = 0;
	int64_t m_firstAudioPts = 0;
	int64_t m_firstAudioDts = 0;

	std::mutex m_queDataMutex;
	std::queue<SData> m_queData;
	std::queue<AVPacket *> m_queAudioPkts;
	std::shared_ptr<MsSocket> m_sock;
	std::shared_ptr<MsReactor> m_reactor;
	std::shared_ptr<MsEvent> m_evt;
	AVFormatContext *m_fmtCtx = nullptr;
	AVIOContext *m_pb = nullptr;
	AVStream *m_outVideo = nullptr;
	AVStream *m_outAudio = nullptr;
	int m_outVideoIdx = 0;
	int m_outAudioIdx = 1;
	bool m_firstPacket = true;
};

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
