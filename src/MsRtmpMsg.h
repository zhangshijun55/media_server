#ifndef MS_RTMP_MSG_H
#define MS_RTMP_MSG_H
#include "MsAmf.h"
#include "MsSocket.h"
#include <cstdint>
#include <vector>

// The amf0 command message, command name macros
#define RTMP_AMF0_COMMAND_CONNECT "connect"
#define RTMP_AMF0_COMMAND_CREATE_STREAM "createStream"
#define RTMP_AMF0_COMMAND_DELETE_STREAM "deleteStream"
#define RTMP_AMF0_COMMAND_ON_BW_DONE "onBWDone"
#define RTMP_AMF0_COMMAND_ON_STATUS "onStatus"
#define RTMP_AMF0_COMMAND_RESULT "_result"
#define RTMP_AMF0_COMMAND_ERROR "_error"
#define RTMP_AMF0_COMMAND_RELEASE_STREAM "releaseStream"
#define RTMP_AMF0_COMMAND_FC_PUBLISH "FCPublish"
#define RTMP_AMF0_COMMAND_UNPUBLISH "FCUnpublish"
#define RTMP_AMF0_COMMAND_PUBLISH "publish"
#define RTMP_AMF0_COMMAND_ON_FC_PUBLISH "onFCPublish"
#define RTMP_AMF0_COMMAND_ON_FC_UNPUBLISH "onFCUnpublish"

#define RTMP_MSG_SetChunkSize 0x01
#define RTMP_MSG_AbortMessage 0x02
#define RTMP_MSG_Acknowledgement 0x03
#define RTMP_MSG_UserControlMessage 0x04
#define RTMP_MSG_WindowAcknowledgementSize 0x05
#define RTMP_MSG_SetPeerBandwidth 0x06

struct RtmpMsg {
	virtual int decode() { return 0; }
	virtual void encode(std::vector<uint8_t> &outBuf, int chunkSize) {}
};

int SendRtmpMsg(MsSocket *sock, RtmpMsg *msg, int chunkSize = 128);

struct RtmpHeader : public RtmpMsg {
	RtmpHeader(const RtmpHeader &) = default;
	RtmpHeader() = default;

	virtual void encode(std::vector<uint8_t> &outBuf, int chunkSize) override;

	uint32_t csid;
	uint8_t fmt;
	uint32_t timestamp;
	uint32_t msgLength;
	uint8_t msgTypeId;
	uint32_t msgStreamId;
	uint32_t timestampDelta;
	std::vector<uint8_t> payload;
	uint32_t payloadBytesRead;
	bool hasExtTs;
};

struct RtmpSizeMsg : public RtmpHeader {
	RtmpSizeMsg(uint32_t sz, uint32_t msgId);
	uint32_t size;
};

struct RtmpSetChunkSize : public RtmpSizeMsg {
	RtmpSetChunkSize(uint32_t sz) : RtmpSizeMsg(sz, RTMP_MSG_SetChunkSize) {}
};

struct RtmpAck : public RtmpSizeMsg {
	RtmpAck(uint32_t sz) : RtmpSizeMsg(sz, RTMP_MSG_Acknowledgement) {}
};

struct RtmpWndAckSize : public RtmpSizeMsg {
	RtmpWndAckSize(uint32_t sz) : RtmpSizeMsg(sz, RTMP_MSG_WindowAcknowledgementSize) {}
};

struct RtmpPeerBandwidth : public RtmpSizeMsg {
	RtmpPeerBandwidth(uint32_t sz, uint8_t t);
	uint8_t type;
};

struct RtmpStreamBegin : public RtmpHeader {
	RtmpStreamBegin(uint32_t streamId);
	uint32_t streamId;
};

struct RtmpConnnectReq : public RtmpHeader {
	RtmpConnnectReq(RtmpHeader &header) : RtmpHeader(header) { argsObj.set_null(); }
	int decode() override;

	string cmdName;
	double transactionId;
	AmfObject cmdObj;
	AmfObject argsObj;
};

// Response  for SrsConnectAppPacket.
struct RtmpConnnectRsp : public RtmpHeader {
	RtmpConnnectRsp() {
		msgTypeId = 20;
		msgStreamId = 0;
		csid = 5;
		timestamp = 0;

		cmdName = RTMP_AMF0_COMMAND_RESULT;
		transactionId = 1;
	}
	void encode(std::vector<uint8_t> &outBuf, int chunkSize) override;

	string cmdName;
	double transactionId;
	AmfObject propObj;
	AmfObject infoObj;
};

struct RtmpCreateStreamReq : public RtmpHeader {
	RtmpCreateStreamReq(RtmpHeader &header) : RtmpHeader(header) {}
	int decode() override;

	string cmdName;
	double transactionId;
	AmfObject commandObject;
};

struct RtmpCreateStreamRsp : public RtmpHeader {
	RtmpCreateStreamRsp(double txnId, double streamId) {
		msgTypeId = 20;
		msgStreamId = 0;
		csid = 5;
		timestamp = 0;

		cmdName = RTMP_AMF0_COMMAND_RESULT;
		transactionId = txnId;
		streamId = streamId;
		commandObject.set_null();
	}

	void encode(std::vector<uint8_t> &outBuf, int chunkSize) override;

	string cmdName;
	double transactionId;
	AmfObject commandObject;
	double streamId;
};

struct RtmpOnBwDone : public RtmpHeader {
	RtmpOnBwDone() {
		msgTypeId = 20;
		msgStreamId = 0;
		csid = 5;
		timestamp = 0;

		cmdName = RTMP_AMF0_COMMAND_ON_BW_DONE;
		transactionId = 0;
		argsObj.set_null();
	}
	void encode(std::vector<uint8_t> &outBuf, int chunkSize) override;

	string cmdName;
	double transactionId;
	AmfObject argsObj;
};

struct RtmpFMLEStartReq : public RtmpHeader {
	RtmpFMLEStartReq(RtmpHeader &header) : RtmpHeader(header) {}
	int decode() override;

	string cmdName;
	double transactionId;
	AmfObject commandObject;
	string streamName;
};

struct RtmpFMLEStartRsp : public RtmpHeader {
	RtmpFMLEStartRsp(double txnId) {
		msgTypeId = 20;
		msgStreamId = 0;
		csid = 5;
		timestamp = 0;

		cmdName = RTMP_AMF0_COMMAND_RESULT;
		transactionId = txnId;
		commandObject.set_null();
		argsObj.set_undefined();
	}

	void encode(std::vector<uint8_t> &outBuf, int chunkSize) override;

	string cmdName;
	double transactionId;
	AmfObject commandObject;
	AmfObject argsObj;
};

struct RtmpPublishReq : public RtmpHeader {
	RtmpPublishReq(RtmpHeader &header) : RtmpHeader(header) {}
	int decode() override;

	std::string cmdName;
	double transactionId;
	AmfObject commandObject;
	std::string streamName;
	std::string type;
};

struct RtmpOnStatusCall : public RtmpHeader {
	RtmpOnStatusCall(uint32_t streamId) {
		msgTypeId = 20;
		msgStreamId = streamId;
		csid = 5;
		timestamp = 0;

		cmdName = RTMP_AMF0_COMMAND_ON_STATUS;
		transactionId = 0;
		argsObj.set_null();
	}

	void encode(std::vector<uint8_t> &outBuf, int chunkSize) override;

	std::string cmdName;
	double transactionId;
	AmfObject argsObj;
	AmfObject dataObj;
};

#endif // MS_RTMP_MSG_H