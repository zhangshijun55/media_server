#ifndef MS_RTMP_HANDLER_H
#define MS_RTMP_HANDLER_H
#include "MsAmf.h"
#include "MsEvent.h"
#include "MsLog.h"
#include "MsRtmpMsg.h"
#include "MsRtmpServer.h"
#include "MsRtmpSource.h"
#include "MsSocket.h"
#include <cstdint>
#include <memory>
#include <unordered_map>

class MsRtmpHandler : public MsEventHandler {
public:
	MsRtmpHandler(shared_ptr<MsRtmpServer> server)
	    : m_server(server), m_buffer(new uint8_t[64 * 1024]), m_bufferSize(64 * 1024),
	      m_dataLength(0), m_handshakeDone(false), m_inChunkSize(128) {}
	~MsRtmpHandler();

	void HandleRead(shared_ptr<MsEvent> evt) override;
	void HandleClose(shared_ptr<MsEvent> evt) override;
	void ProcessBuffer(shared_ptr<MsEvent> evt);

public:
	unique_ptr<uint8_t[]> m_buffer;
	size_t m_bufferSize = 0;
	size_t m_dataLength = 0;
	uint32_t m_recvBytes = 0;
	uint32_t m_deltaRecv = 0;

private:
	void PareseRtmpMessage(RtmpHeader &header, shared_ptr<MsEvent> evt);
	void ProcessConnect(RtmpHeader &header, shared_ptr<MsEvent> evt);
	void ProcessCreateStream(RtmpHeader &header, shared_ptr<MsEvent> evt);
	void ProcessReleaseStream(RtmpHeader &header, shared_ptr<MsEvent> evt);
	void ProcessPublish(RtmpHeader &header, shared_ptr<MsEvent> evt);
	void ProcessFCUnpublish(RtmpHeader &header, shared_ptr<MsEvent> evt);

	uint32_t m_ackWndSize = 2500000;
	bool m_handshakeDone = false;
	bool m_s2Sent = false;
	string m_clientRandomBytes;
	uint32_t m_inChunkSize;
	std::unordered_map<uint32_t, RtmpHeader> m_chunkMap;
	shared_ptr<MsRtmpSource> m_rtmpSource;

	shared_ptr<MsRtmpServer> m_server;
};

#endif // MS_RTMP_HANDLER_H