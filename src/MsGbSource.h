#pragma once
#include "MsCommon.h"
#include "MsMediaSource.h"
#include "MsMsgDef.h"
#include "MsReactor.h"
#include "MsRingBuffer.h"
#include <condition_variable>
#include <mutex>
#include <thread>

class MsGbSource : public MsMediaSource, public MsReactor {
public:
	MsGbSource(const std::string &streamID, SGbContext *ctx, int id)
	    : MsMediaSource(streamID), MsReactor(MS_GB_SOURCE, id), m_ctx(ctx),
	      m_ringBuffer(std::make_unique<MsRingBuffer>(DEF_BUF_SIZE)) {}

	~MsGbSource() {
		if (m_ctx) {
			delete m_ctx;
		}
	}

	void Work() override;
	shared_ptr<MsMediaSource> GetSharedPtr() override {
		return dynamic_pointer_cast<MsMediaSource>(shared_from_this());
	}

	void Exit() override;
	void HandleMsg(MsMsg &msg) override;
	int ProcessRtp(uint8_t *buf, int len);

	void SourceActiveClose() override;
	void OnSinksEmpty() override;

private:
	void OnRun();
	void PsParseThread();
	int ReadBuffer(uint8_t *buf, int buf_size);
	int WriteBuffer(uint8_t *buf, int len);

private:
	bool m_firstPkt = true;
	uint16_t m_seq = 0;
	int m_payload = 96;

	std::unique_ptr<MsRingBuffer> m_ringBuffer;
	std::mutex m_mutex;
	std::condition_variable m_condVar;
	std::unique_ptr<std::thread> m_psThread;

	SGbContext *m_ctx;
	shared_ptr<MsSocket> m_rtpSock; // for tcp active
};
