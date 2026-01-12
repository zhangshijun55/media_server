#ifndef MS_MEDIA_SINK_H
#define MS_MEDIA_SINK_H

#include "MsLog.h"
#include <string>

extern "C" {
#include <libavformat/avformat.h>
}

class MsMediaSink {
public:
	MsMediaSink(const std::string &type, const std::string &streamID, int sinkID)
	    : m_type(type), m_streamID(streamID), m_sinkID(sinkID) {}
	virtual ~MsMediaSink() { MS_LOG_INFO("media sink %s-%d destroyed", m_type.c_str(), m_sinkID); }

	virtual void DetachSource();
	virtual void DetachSourceNoLock();

	virtual void OnStreamInfo(AVStream *video, int videoIdx, AVStream *audio, int audioIdx);

	virtual void OnSourceClose() = 0;
	virtual void OnStreamPacket(AVPacket *pkt) = 0;

public:
	std::string m_type;
	std::string m_streamID;
	int m_sinkID;

protected:
	AVStream *m_video = nullptr;
	int m_videoIdx = -1;
	AVStream *m_audio = nullptr;
	int m_audioIdx = -1;
};

#endif // MS_MEDIA_SINK_H