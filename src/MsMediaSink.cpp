#include "MsMediaSink.h"
#include "MsResManager.h"

void MsMediaSink::DetachSource() {
	auto source = MsResManager::GetInstance().GetMediaSource(m_streamID);
	if (source)
		source->RemoveSink(m_type, m_sinkID);
}

void MsMediaSink::DetachSourceNoLock() {
	auto source = MsResManager::GetInstance().GetMediaSource(m_streamID);
	if (source)
		source->RemoveSinkNoLock(m_type, m_sinkID);
}

void MsMediaSink::OnStreamInfo(AVStream *video, int videoIdx, AVStream *audio, int audioIdx) {
	m_video = video;
	m_videoIdx = videoIdx;
	m_audio = audio;
	m_audioIdx = audioIdx;
}
