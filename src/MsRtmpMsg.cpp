#include "MsRtmpMsg.h"
#include "MsLog.h"

void RtmpHeader::encode(std::vector<uint8_t> &outBuf, int chunkSize) {
	// Basic Header
	fmt = 0; // first chunk
	msgLength = payload.size();

	uint8_t fmt_csid = (fmt << 6) | (csid & 0x3F);
	outBuf.push_back(fmt_csid);

	// Message Header
	outBuf.push_back((timestamp >> 16) & 0xFF);
	outBuf.push_back((timestamp >> 8) & 0xFF);
	outBuf.push_back(timestamp & 0xFF);

	outBuf.push_back((msgLength >> 16) & 0xFF);
	outBuf.push_back((msgLength >> 8) & 0xFF);
	outBuf.push_back(msgLength & 0xFF);

	outBuf.push_back(msgTypeId);

	outBuf.push_back(msgStreamId & 0xFF);
	outBuf.push_back((msgStreamId >> 8) & 0xFF);
	outBuf.push_back((msgStreamId >> 16) & 0xFF);
	outBuf.push_back((msgStreamId >> 24) & 0xFF);

	// Payload
	size_t payloadSize = payload.size();
	size_t offset = 0;
	while (offset < payloadSize) {
		size_t toWrite = std::min((size_t)chunkSize, payloadSize - offset);
		if (offset > 0) {
			uint8_t sep = (3 << 6) | (csid & 0x3F); // fmt type 3 for continuation
			outBuf.push_back(sep);
		}
		outBuf.insert(outBuf.end(), payload.begin() + offset, payload.begin() + offset + toWrite);
		offset += toWrite;
	}
}

RtmpSizeMsg::RtmpSizeMsg(uint32_t sz, uint32_t msgId) : size(sz) {
	csid = 2;
	timestamp = 0;
	msgTypeId = msgId;
	msgStreamId = 0;

	payload.resize(4);
	payload[0] = (size >> 24) & 0xFF;
	payload[1] = (size >> 16) & 0xFF;
	payload[2] = (size >> 8) & 0xFF;
	payload[3] = size & 0xFF;
}

RtmpPeerBandwidth::RtmpPeerBandwidth(uint32_t sz, uint8_t t)
    : RtmpSizeMsg(sz, RTMP_MSG_SetPeerBandwidth), type(t) {
	payload.resize(5);
	payload[4] = type;
}

RtmpStreamBegin::RtmpStreamBegin(uint32_t streamId) : streamId(streamId) {
	csid = 2;
	timestamp = 0;
	msgTypeId = RTMP_MSG_UserControlMessage; // User Control Message
	msgStreamId = 0;

	payload.resize(6);
	payload[0] = 0x00; // Event Type: Stream Begin (0x00 0x00)
	payload[1] = 0x00;
	payload[2] = (streamId >> 24) & 0xFF;
	payload[3] = (streamId >> 16) & 0xFF;
	payload[4] = (streamId >> 8) & 0xFF;
	payload[5] = streamId & 0xFF;
}

int SendRtmpMsg(MsSocket *sock, RtmpMsg *msg, int chunkSize) {
	std::vector<uint8_t> outBuf;
	msg->encode(outBuf, chunkSize);
	return sock->Send((const char *)outBuf.data(), outBuf.size());
}

int RtmpConnnectReq::decode() {
	uint8_t *data = payload.data();
	size_t len = payload.size();
	size_t offset = 0;

	if (amf_decode_string(data, len, offset, cmdName) < 0) {
		MS_LOG_ERROR("Failed to decode connect request");
		return -1;
	}

	if (amf_decode_number(data, len, offset, transactionId) < 0) {
		MS_LOG_ERROR("Failed to decode transaction ID");
		return -1;
	}

	if (transactionId != 1) {
		MS_LOG_WARN("Unexpected transaction ID: %.2f", transactionId);
		transactionId = 1;
	}

	if (cmdObj.decode(data, len, offset) < 0) {
		MS_LOG_ERROR("Failed to decode command object");
		return -1;
	}

	argsObj.decode(data, len, offset);

	return 0;
}

void RtmpConnnectRsp::encode(std::vector<uint8_t> &outBuf, int chunkSize) {
	amf_encode_string(this->payload, cmdName);
	amf_encode_number(this->payload, transactionId);
	propObj.encode(this->payload);
	infoObj.encode(this->payload);

	RtmpHeader::encode(outBuf, chunkSize);
}

int RtmpCreateStreamReq::decode() {
	uint8_t *data = payload.data();
	size_t len = payload.size();
	size_t offset = 0;

	if (amf_decode_string(data, len, offset, cmdName) < 0) {
		MS_LOG_ERROR("Failed to decode createStream request");
		return -1;
	}

	if (amf_decode_number(data, len, offset, transactionId) < 0) {
		MS_LOG_ERROR("Failed to decode transaction ID");
		return -1;
	}

	if (commandObject.decode(data, len, offset) < 0) {
		MS_LOG_ERROR("Failed to decode command object");
		return -1;
	}

	return 0;
}

void RtmpCreateStreamRsp::encode(std::vector<uint8_t> &outBuf, int chunkSize) {
	amf_encode_string(this->payload, cmdName);
	amf_encode_number(this->payload, transactionId);
	commandObject.encode(this->payload);
	amf_encode_number(this->payload, streamId);

	RtmpHeader::encode(outBuf, chunkSize);
}

void RtmpOnBwDone::encode(std::vector<uint8_t> &outBuf, int chunkSize) {
	amf_encode_string(this->payload, cmdName);
	amf_encode_number(this->payload, transactionId);
	argsObj.encode(this->payload);

	RtmpHeader::encode(outBuf, chunkSize);
}

int RtmpFMLEStartReq::decode() {
	uint8_t *data = payload.data();
	size_t len = payload.size();
	size_t offset = 0;
	if (amf_decode_string(data, len, offset, cmdName) < 0) {
		MS_LOG_ERROR("Failed to decode releaseStream request");
		return -1;
	}
	if (amf_decode_number(data, len, offset, transactionId) < 0) {
		MS_LOG_ERROR("Failed to decode transaction ID");
		return -1;
	}
	if (commandObject.decode(data, len, offset) < 0) {
		MS_LOG_ERROR("Failed to decode command object");
		return -1;
	}
	if (amf_decode_string(data, len, offset, streamName) < 0) {
		MS_LOG_ERROR("Failed to decode stream name");
		return -1;
	}

	return 0;
}

void RtmpFMLEStartRsp::encode(std::vector<uint8_t> &outBuf, int chunkSize) {
	amf_encode_string(this->payload, cmdName);
	amf_encode_number(this->payload, transactionId);
	commandObject.encode(this->payload);
	argsObj.encode(this->payload);

	RtmpHeader::encode(outBuf, chunkSize);
}

int RtmpPublishReq::decode() {
	uint8_t *data = payload.data();
	size_t len = payload.size();
	size_t offset = 0;

	if (amf_decode_string(data, len, offset, cmdName) < 0) {
		MS_LOG_ERROR("Failed to decode publish request");
		return -1;
	}
	if (amf_decode_number(data, len, offset, transactionId) < 0) {
		MS_LOG_ERROR("Failed to decode transaction ID");
		return -1;
	}
	if (commandObject.decode(data, len, offset) < 0) {
		MS_LOG_ERROR("Failed to decode command object");
		return -1;
	}
	if (amf_decode_string(data, len, offset, streamName) < 0) {
		MS_LOG_ERROR("Failed to decode stream name");
		return -1;
	}
	if (offset < len) {
		if (amf_decode_string(data, len, offset, type) < 0) {
			MS_LOG_ERROR("Failed to decode type");
			return -1;
		}
	}
	return 0;
}

void RtmpOnStatusCall::encode(std::vector<uint8_t> &outBuf, int chunkSize) {
	amf_encode_string(this->payload, cmdName);
	amf_encode_number(this->payload, transactionId);
	argsObj.encode(this->payload);
	dataObj.encode(this->payload);

	RtmpHeader::encode(outBuf, chunkSize);
}
