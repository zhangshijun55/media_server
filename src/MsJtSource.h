#ifndef MS_JT_SOURCE_H
#define MS_JT_SOURCE_H
#include "MsJtServer.h"
#include "MsMediaSource.h"
#include "MsMsgDef.h"
#include <queue>
#include <string>
#include <thread>

class MsJtSource : public MsMediaSource, public enable_shared_from_this<MsMediaSource> {
public:
	MsJtSource(const string &streamId) : MsMediaSource(streamId) {
		// streamId format: terminalId_channelId_streamType_jt
		auto vec = SplitString(streamId, "_");
		m_terminalId = vec[0];
		if (vec.size() >= 4) {
			m_channelId = stoi(vec[1]);
			m_streamType = stoi(vec[2]);
		}
	}

	void Work() override;
	shared_ptr<MsMediaSource> GetSharedPtr() override { return shared_from_this(); }

	void SourceActiveClose() override;
	void OnSinksEmpty() override;

	void CloseAcceptEvent();
	void CloseTcpEvent();
	void CloseUdpEvent();
	void OnTcpEvent(shared_ptr<MsEvent> evt);

	void PrepareRtp(uint8_t dataType, uint16_t seq, uint64_t timestamp, uint8_t *data, int bodyLen);
	int ReadBuffer(uint8_t *buf, int buf_size);

private:
	void OnStreamInfo(shared_ptr<SJtStreamInfo> avAttr);
	void DemuxThread();
	void SourceReleaseRes();

private:
	bool m_demuxReady = false;
	string m_ip;
	int m_port = 0;

	string m_terminalId;
	uint8_t m_channelId = 1;
	uint8_t m_streamType = 0; // 0: main, 1: sub

	uint8_t m_oriVideoPt = 0;
	uint8_t m_oriAudioPt = 0;
	uint8_t m_videoPt = 0;
	uint8_t m_audioPt = 0;
	uint8_t m_audioChannels = 0;
	double m_audioClock = 0.0;
	string m_videoCodec;
	string m_audioCodec;
	shared_ptr<MsEvent> m_acceptEvent;
	shared_ptr<MsEvent> m_tcpEvent;
	shared_ptr<MsEvent> m_udpEvent;

	std::queue<SData> m_rtpDataQue;
	std::queue<SData> m_recyleQue;
	std::mutex m_queMtx;
	std::condition_variable m_condVar;

	string m_sdp;
	int m_readSdpPos = 0;
	std::unique_ptr<std::thread> m_demuxThread;
	shared_ptr<MsJtServer> m_server;
};
#endif // MS_JT_SOURCE_H
