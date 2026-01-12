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

	struct SRtcPeerConn : public MsMediaSource, public enable_shared_from_this<SRtcPeerConn> {
		SRtcPeerConn(const string &sessionId) : MsMediaSource(sessionId), _sessionId(sessionId) {}
		~SRtcPeerConn();

		void Work() override {}
		void AddSink(std::shared_ptr<MsMediaSink> sink) override;
		void NotifyStreamPacket(AVPacket *pkt) override;
		void SourceActiveClose() override;
		void OnSinksEmpty() override {}
		shared_ptr<MsMediaSource> GetSharedPtr() override { return shared_from_this(); }

		void GenerateSdp();
		void StartRtpDemux();
		void WriteBuffer(const void *buf, int size);
		int ReadBuffer(uint8_t *buf, int buf_size);

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
		std::queue<SData> m_rtcDataQue;
		std::queue<SData> m_recyleQue;
		std::mutex m_queMtx;
		std::condition_variable m_condVar;
		std::unique_ptr<std::thread> m_rtpThread;
	};

	std::mutex m_mtx;
	std::map<string, shared_ptr<SRtcPeerConn>> m_pcMap;
};
