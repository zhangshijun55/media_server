#ifndef MS_HTTP_SINK_H
#define MS_HTTP_SINK_H

#include "MsEvent.h"
#include "MsMediaSink.h"
#include "MsMsgDef.h"
#include "MsReactor.h"

class MsHttpSink : public MsMediaSink, public MsEventHandler {
public:
	MsHttpSink(const std::string &type, const std::string &streamID, int sinkID,
	           std::shared_ptr<MsSocket> sock)
	    : MsMediaSink(type, streamID, sinkID), m_sock(sock) {}

	void HandleRead(shared_ptr<MsEvent> evt) override;
	void HandleClose(shared_ptr<MsEvent> evt) override;
	void HandleWrite(shared_ptr<MsEvent> evt) override;

	void OnStreamInfo(AVStream *video, int videoIdx, AVStream *audio, int audioIdx) override;
	void OnSourceClose() override;
	void OnStreamPacket(AVPacket *pkt) override;

	int WriteBuffer(const uint8_t *buf, int buf_size);

private:
	void ReleaseResources();
	void PassiveClose();
	void SinkActiveClose();
	void clear_que();

	bool m_streamReady = false;
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

#endif // MS_HTTP_SINK_H