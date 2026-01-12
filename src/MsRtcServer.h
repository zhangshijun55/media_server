#pragma once

#include "MsHttpMsg.h"
#include "MsLog.h"
#include "MsMsgDef.h"
#include "MsReactor.h"
#include "MsResManager.h"
#include "MsSocket.h"

#include "rtc/rtc.hpp"
#include <map>
#include <mutex>
#include <sstream>
#include <string.h>

class MsRtcServer : public MsReactor {
public:
	using MsReactor::MsReactor;

	void Run() override;
	void HandleMsg(MsMsg &msg) override;

private:
	void RtcProcess(SHttpTransferMsg *rtcMsg);
	void WhipProcess(SHttpTransferMsg *rtcMsg);

	struct SRtcPeerConn : public MsMediaSource {
		SRtcPeerConn(const string &sessionId) : MsMediaSource(sessionId), _sessionId(sessionId) {}
		~SRtcPeerConn() { MS_LOG_INFO("~SRtcPeerConn"); }

		void Work() override {}
		shared_ptr<MsMediaSource> GetSharedPtr() override { return nullptr; }

		void GenerateSdp() {
			std::stringstream ss;
			ss << "v=0\r\n";
			ss << "o=- 0 0 IN IP4 127.0.0.1\r\n";
			ss << "c=IN IP4 127.0.0.1\r\n";

			if (_videoPt > 0) {
				ss << "m=video 0 /RTP/AVP " << _videoPt << "\r\n";
				ss << "a=rtpmap:" << _videoPt << " " << _videoCodec << "/90000\r\n";
				for (size_t i = 0; i < _videoFmts.size(); ++i) {
					ss << "a=fmtp:" << _videoPt << " " << _videoFmts[i].substr(strlen("fmtp:"));
					ss << "\r\n";
				}
			}

			if (_audioPt > 0) {
				ss << "m=audio 0 RTP/AVP " << _audioPt << "\r\n";
				int clockRate = 48000;
				ss << "a=rtpmap:" << _audioPt << " " << _audioCodec << "/" << clockRate << "/2\r\n";
				for (size_t i = 0; i < _audioFmts.size(); ++i) {
					ss << "a=fmtp:" << _audioPt << " " << _audioFmts[i].substr(strlen("fmtp:"));
					ss << "\r\n";
				}
			}

			_sdp = ss.str();
		}

		void StartRtpDemux();

		shared_ptr<rtc::PeerConnection> _pc;
		shared_ptr<rtc::Track> _videoTrack;
		shared_ptr<rtc::Track> _audioTrack;
		shared_ptr<MsSocket> _sock;
		int _videoPt = 0;
		int _audioPt = 0;
		int _expectedTracks = 0;
		int _receivedTracks = 0;
		int _readSdpPos = 0;
		string _sessionId;
		string _videoCodec;
		string _audioCodec;
		string _sdp;
		vector<string> _videoFmts;
		vector<string> _audioFmts;
	};

	std::queue<SData> m_rtcDataQue;
	std::queue<SData> m_recyleQue;
	std::mutex m_queMtx;

	void WirteBuffer(const void *buf, int size) {
		std::lock_guard<std::mutex> lock(m_queMtx);
		if (m_recyleQue.size() > 0) {
			SData &sd = m_recyleQue.front();
			if (sd.m_capacity >= size) {
				memcpy(sd.m_buf, buf, size);
				sd.m_len = size;
				m_rtcDataQue.push(sd);
				m_recyleQue.pop();
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
	}

	int ReadBuffer(uint8_t *buf, int buf_size) {
		std::lock_guard<std::mutex> lock(m_queMtx);

		if (m_rtcDataQue.size() == 0) {
			return -1;
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

	std::mutex m_mtx;
	std::map<string, shared_ptr<SRtcPeerConn>> m_pcMap;
};