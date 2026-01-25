#ifndef MS_RTMPSOURCE_H
#define MS_RTMPSOURCE_H
#include "MsMediaSource.h"
#include "MsRingBuffer.h"
#include <condition_variable>
#include <mutex>
#include <thread>

class MsRtmpSource : public MsMediaSource, public std::enable_shared_from_this<MsRtmpSource> {
public:
	MsRtmpSource(const string &streamName) : MsMediaSource(streamName) {}

	void Work() override;
	shared_ptr<MsMediaSource> GetSharedPtr() override { return shared_from_this(); }

	void AddSink(std::shared_ptr<MsMediaSink> sink) override;
	void NotifyStreamPacket(AVPacket *pkt) override;
	void SourceActiveClose() override;
	void OnSinksEmpty() override {}

	int ReadBuffer(uint8_t *buf, int buf_size);
	int WirteFlvData(uint8_t msgId, uint32_t msgLength, uint32_t timestamp, uint8_t *data);

private:
	void FlvParseThread();

private:
	std::unique_ptr<std::thread> m_flvThread;
	std::mutex m_mutex;
	std::condition_variable m_condVar;
	std::unique_ptr<MsRingBuffer> m_ringBuffer{std::make_unique<MsRingBuffer>(64 * 1024)};
};

#endif // MS_RTMPSOURCE_H