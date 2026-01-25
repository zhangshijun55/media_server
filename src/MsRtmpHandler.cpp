#include "MsRtmpHandler.h"
#include "MsCommon.h"
#include "MsLog.h"
#include "MsResManager.h"
#include "MsSocket.h"
#include <algorithm>
#include <cstdint>
#include <cstring>

MsRtmpHandler::~MsRtmpHandler() { MS_LOG_INFO("~MsRtmpHandler"); }

void MsRtmpHandler::HandleRead(shared_ptr<MsEvent> evt) {
	MsSocket *sock = evt->GetSocket();

	ssize_t bytesRead =
	    sock->Recv((char *)m_buffer.get() + m_dataLength, m_bufferSize - m_dataLength);
	if (bytesRead > 0) {
		m_dataLength += bytesRead;
		m_recvBytes += bytesRead;
		m_deltaRecv += bytesRead;

		if (m_deltaRecv >= m_ackWndSize) {
			RtmpAck ackMsg(m_recvBytes);
			SendRtmpMsg(sock, &ackMsg, m_inChunkSize);
			m_deltaRecv = 0;
		}

		ProcessBuffer(evt);
	} else if (bytesRead == 0) {
		MS_LOG_INFO("MsRtmpHandler::HandleRead - Connection closed by peer");
	}
}

void MsRtmpHandler::HandleClose(shared_ptr<MsEvent> evt) {
	MS_LOG_INFO("MsRtmpHandler::HandleClose - RTMP connection closed");
	// Handle cleanup of RTMP connection resources here

	if (m_rtmpSource) {
		m_server->UnregistSource(m_rtmpSource->GetStreamID().substr(
		    0, m_rtmpSource->GetStreamID().length() - 5)); // remove "_rtmp" suffix
		m_rtmpSource->SourceActiveClose();
		m_rtmpSource = nullptr;
	}

	m_server->DelEvent(evt);
}

void MsRtmpHandler::ProcessBuffer(shared_ptr<MsEvent> evt) {
	// Process the RTMP data in m_buffer
	if (!m_handshakeDone) {
		if (!m_s2Sent && m_dataLength >= 1537) {
			uint8_t *data = m_buffer.get();
			if (data[0] != 0x03) {
				MS_LOG_ERROR("MsRtmpHandler::ProcessBuffer - Invalid RTMP version");
				return;
			}

			// Send S0 + S1 + S2
			size_t sSize = 1 + 1536 + 1536;
			unique_ptr<char[]> sBuffer = make_unique<char[]>(sSize);
			sBuffer[0] = 0x03; // S0

			m_clientRandomBytes = GenRandStr(16);
			memset(sBuffer.get() + 1, 0x00, 1536);                         // S1
			memcpy(sBuffer.get() + 1 + 8, m_clientRandomBytes.data(), 16); // S1 random bytes
			memcpy(sBuffer.get() + 1 + 1536, data + 1, 1536);              // S2 (echo C1)

			evt->GetSocket()->Send(sBuffer.get(), sSize);

			// Remove C0 + C1 from buffer
			memmove(m_buffer.get(), m_buffer.get() + 1537, m_dataLength - 1537);
			m_dataLength -= 1537;
			m_s2Sent = true;

			MS_LOG_INFO("MsRtmpHandler::ProcessBuffer - Handshake S0/S1/S2 sent");
		} else if (m_s2Sent && m_dataLength >= 1536) {
			// comapre C2 with S1
			uint8_t *data = m_buffer.get();
			if (memcmp(data + 8, m_clientRandomBytes.data(), 16) != 0) {
				MS_LOG_ERROR("MsRtmpHandler::ProcessBuffer - C2 does not match S1");
				m_server->DelEvent(evt);
				return;
			}

			MS_LOG_DEBUG("MsRtmpHandler::ProcessBuffer - Handshake completed");

			m_handshakeDone = true;
			memmove(m_buffer.get(), m_buffer.get() + 1536, m_dataLength - 1536);
			m_dataLength -= 1536;
		} else {
			// Wait for more data
			return;
		}
	}
	// Further RTMP message processing would go here after handshake is done
	while (m_dataLength > 0) {
		uint8_t *data = m_buffer.get();
		size_t offset = 0;

		// 1. Basic Header
		uint8_t fmt = (data[0] >> 6) & 0x03;
		uint32_t csid = data[0] & 0x3F;
		size_t basicHeaderLen = 1;

		if (csid == 0) {
			if (m_dataLength < 2)
				return;
			csid = 64 + data[1];
			basicHeaderLen = 2;
		} else if (csid == 1) {
			if (m_dataLength < 3)
				return;
			csid = 64 + data[1] + ((uint32_t)data[2] << 8);
			basicHeaderLen = 3;
		}
		offset += basicHeaderLen;

		// 2. Message Header
		size_t msgHeaderLen = 0;
		if (fmt == 0)
			msgHeaderLen = 11;
		else if (fmt == 1)
			msgHeaderLen = 7;
		else if (fmt == 2)
			msgHeaderLen = 3;

		if (m_dataLength < offset + msgHeaderLen)
			return;

		RtmpHeader &header = m_chunkMap[csid];
		// uint8_t previousFmt = header.fmt;
		header.fmt = fmt;
		header.csid = csid;
		bool addDelta = false;
		// if the header isn't consumed, the added delta should be decreased back

		if (fmt == 0) {
			uint32_t ts = (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2];
			header.msgLength =
			    (data[offset + 3] << 16) | (data[offset + 4] << 8) | data[offset + 5];
			header.msgTypeId = data[offset + 6];
			header.msgStreamId = (data[offset + 7]) | (data[offset + 8] << 8) |
			                     (data[offset + 9] << 16) | (data[offset + 10] << 24);
			header.hasExtTs = (ts == 0xFFFFFF);
			header.timestamp = ts;
			header.timestampDelta = 0;
		} else if (fmt == 1) {
			uint32_t delta = (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2];
			header.msgLength =
			    (data[offset + 3] << 16) | (data[offset + 4] << 8) | data[offset + 5];
			header.msgTypeId = data[offset + 6];
			header.hasExtTs = (delta == 0xFFFFFF);
			header.timestampDelta = delta;
			header.timestamp += delta;
			addDelta = true;
		} else if (fmt == 2) {
			uint32_t delta = (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2];
			header.hasExtTs = (delta == 0xFFFFFF);
			header.timestampDelta = delta;
			header.timestamp += delta;
			addDelta = true;
		} else if (fmt == 3) {
			if (header.payloadBytesRead == 0) { // first chunk of this message
				// if (previousFmt == 0) {
				// 	// refer to 5.3.1.2.4
				// 	MS_LOG_DEBUG("Chunk xxxx received: csid=%d, msgTypeId=%d, timestampDelta=%d",
				// 	             header.csid, header.msgTypeId, header.timestampDelta);
				// 	header.timestampDelta = header.timestamp;
				// }

				// MS_LOG_DEBUG(
				//     "Chunk yyyy received: csid=%d, msgTypeId=%d, timestampDelta=%d, prevFmt=%d",
				//     header.csid, header.msgTypeId, header.timestampDelta, previousFmt);

				header.timestamp += header.timestampDelta;
				addDelta = true;
			}
		}
		offset += msgHeaderLen;

		// 3. Extended Timestamp
		if (header.hasExtTs) {
			if (m_dataLength < offset + 4) {
				if (addDelta) {
					// MS_LOG_DEBUG("Chunk received: csid=%d, msgTypeId=%d, timestampDelta=%d",
					//              header.csid, header.msgTypeId, header.timestampDelta);
					header.timestamp -= header.timestampDelta;
				}
				return;
			}
			uint32_t extTs = (data[offset] << 24) | (data[offset + 1] << 16) |
			                 (data[offset + 2] << 8) | data[offset + 3];
			if (fmt == 0)
				header.timestamp = extTs;
			else {
				header.timestamp -= 0xFFFFFF;
				header.timestamp += extTs;
				if (fmt == 1 || fmt == 2)
					header.timestampDelta = extTs;
			}
			offset += 4;
		}

		// 4. Payload
		size_t chunkSize =
		    std::min((size_t)m_inChunkSize, (size_t)(header.msgLength - header.payloadBytesRead));

		if (m_dataLength < offset + chunkSize) {
			if (addDelta) {
				// MS_LOG_DEBUG("Chunk received: csid=%d, msgTypeId=%d, timestampDelta=%d",
				//              header.csid, header.msgTypeId, header.timestampDelta);
				header.timestamp -= header.timestampDelta;
			}
			return;
		}

		if (header.payload.size() < header.msgLength) {
			header.payload.resize(header.msgLength);
		}

		if (chunkSize > 0) {
			memcpy(header.payload.data() + header.payloadBytesRead, data + offset, chunkSize);
			header.payloadBytesRead += chunkSize;
			offset += chunkSize;
		} else {
			if (addDelta) {
				// MS_LOG_DEBUG("Chunk received: csid=%d, msgTypeId=%d, timestampDelta=%d",
				//              header.csid, header.msgTypeId, header.timestampDelta);
				header.timestamp -= header.timestampDelta;
			}
		}

		// Check if message is complete
		if (header.payloadBytesRead >= header.msgLength) {
			// if (header.csid == 6) {
			// MS_LOG_INFO("Recv RTMP Msg: fmt:%d type=%d len=%d csid=%d msid=%d time=%d
			// timedelta=%d",
			//             header.fmt, header.msgTypeId, header.msgLength, header.csid,
			//             header.msgStreamId, header.timestamp, header.timestampDelta);
			//}

			if (header.msgTypeId == 1) {
				uint32_t newSize = (header.payload[0] << 24) | (header.payload[1] << 16) |
				                   (header.payload[2] << 8) | header.payload[3];
				m_inChunkSize = newSize & 0x7FFFFFFF;
				MS_LOG_DEBUG("Set Chunk Size to %d", m_inChunkSize);
			} else if (header.msgTypeId == 2) {
				// abort message, ignore
				MS_LOG_DEBUG("Received Abort Message for csid %d",
				             (header.payload[0] | (header.payload[1] << 8) |
				              (header.payload[2] << 16) | (header.payload[3] << 24)));
			} else if (header.msgTypeId == 3) {
				// acknowledgement
				// ignore for now
			} else if (header.msgTypeId == 5) {
				// window acknowledgement size
				uint32_t ackSize = (header.payload[0] << 24) | (header.payload[1] << 16) |
				                   (header.payload[2] << 8) | header.payload[3];
				MS_LOG_DEBUG("Set Acknowledgement Window Size to %d", ackSize);

				if (m_deltaRecv >= ackSize) {
					RtmpAck ackMsg(m_recvBytes);
					SendRtmpMsg(evt->GetSocket(), &ackMsg, m_inChunkSize);
					m_deltaRecv = 0;
				}

				m_ackWndSize = ackSize;

			} else if (header.msgTypeId == 6) {
				// peer bandwidth
				// ignore for now
				MS_LOG_DEBUG("Received Peer Bandwidth Message");
			} else {
				// Process other RTMP messages
				PareseRtmpMessage(header, evt);
			}

			header.payloadBytesRead = 0;
			// reuse buffer, no need to clear, is there any issue?
			// header.payload.clear();
		}

		memmove(m_buffer.get(), m_buffer.get() + offset, m_dataLength - offset);
		m_dataLength -= offset;
	}
}

void MsRtmpHandler::PareseRtmpMessage(RtmpHeader &header, shared_ptr<MsEvent> evt) {
	const uint8_t *data = header.payload.data();
	size_t len = header.msgLength; // header.payload.size();
	size_t offset = 0;

	if (header.msgTypeId == 20 || header.msgTypeId == 17) { // Command Message
		if (header.msgTypeId == 17) {
			if (len < 1 || data[0] != 0) {
				MS_LOG_ERROR("Invalid AMF0 command message format:%d", data[0]);
				return;
			}
			offset++;
		}

		// AMF0 Command Name
		string cmdName;
		if (amf_decode_string(data, len, offset, cmdName) < 0) {
			MS_LOG_ERROR("Failed to decode AMF0 command name");
			return;
		}

		if (cmdName == RTMP_AMF0_COMMAND_CONNECT) {
			this->ProcessConnect(header, evt);
		} else if (cmdName == RTMP_AMF0_COMMAND_RELEASE_STREAM) {
			// Handle releaseStream
			this->ProcessReleaseStream(header, evt);
		} else if (cmdName == RTMP_AMF0_COMMAND_FC_PUBLISH) {
			// Handle FCPublish
			this->ProcessReleaseStream(header, evt);
		} else if (cmdName == RTMP_AMF0_COMMAND_UNPUBLISH) {
			// Handle unpublish
			this->ProcessFCUnpublish(header, evt);
		} else if (cmdName == RTMP_AMF0_COMMAND_CREATE_STREAM) {
			this->ProcessCreateStream(header, evt);
		} else if (cmdName == RTMP_AMF0_COMMAND_PUBLISH) {
			// Handle publish
			this->ProcessPublish(header, evt);
		} else if (cmdName == RTMP_AMF0_COMMAND_DELETE_STREAM) {
			// Handle deleteStream
			MS_LOG_WARN("deleteStream command received, ignored for now");
		} else {
			MS_LOG_WARN("Unhandled RTMP command: %s", cmdName.c_str());
		}
	} else if (header.msgTypeId == 8 || header.msgTypeId == 9 || header.msgTypeId == 18) {
		if (m_rtmpSource) {
			m_rtmpSource->WirteFlvData(header.msgTypeId, header.msgLength, header.timestamp,
			                           header.payload.data());
		}

	} else {
		MS_LOG_WARN("Unhandled RTMP message type: %d", header.msgTypeId);
	}
}

void MsRtmpHandler::ProcessConnect(RtmpHeader &header, shared_ptr<MsEvent> evt) {
	// Implementation of connect command processing
	auto req = make_unique<RtmpConnnectReq>(header);
	if (req->decode() < 0) {
		MS_LOG_ERROR("Failed to decode connect request");
		return;
	}

	MS_LOG_DEBUG("Processing transaction id: %.2f cmd:%s", req->transactionId,
	             req->cmdName.c_str());

	string app = req->cmdObj.get_key_string("app");
	string tcUrl = req->cmdObj.get_key_string("tcUrl");
	MS_LOG_INFO("Client connecting to app: %s, tcUrl: %s", app.c_str(), tcUrl.c_str());
	if (app != "live") {
		MS_LOG_WARN("Unexpected app name: %s", app.c_str());
		// return error to client
		RtmpOnStatusCall errorMsg(0);
		errorMsg.cmdName = RTMP_AMF0_COMMAND_ON_STATUS;
		errorMsg.dataObj.set_key_string("level", "error");
		errorMsg.dataObj.set_key_string("code", "NetConnection.Connect.Rejected");
		errorMsg.dataObj.set_key_string("description", "App name not recognized.");
		SendRtmpMsg(evt->GetSocket(), &errorMsg, m_inChunkSize);
		return;
	}

	RtmpWndAckSize wndAckMsg(2500000);
	SendRtmpMsg(evt->GetSocket(), &wndAckMsg, m_inChunkSize);

	RtmpPeerBandwidth peerBwMsg(2500000, 2);
	SendRtmpMsg(evt->GetSocket(), &peerBwMsg, m_inChunkSize);

	// after set chunk size to client, respond with larger chunk size
	RtmpSetChunkSize setChunkMsg(4096);
	SendRtmpMsg(evt->GetSocket(), &setChunkMsg, m_inChunkSize);
	m_inChunkSize = 4096;

	// send _result
	RtmpConnnectRsp rspMsg;
	rspMsg.propObj.set_key_string("fmsVer", "FMS/3,5,3,888");
	rspMsg.propObj.set_key_number("capabilities", 127);
	rspMsg.propObj.set_key_number("mode", 1);

	rspMsg.infoObj.set_key_string("level", "status");
	rspMsg.infoObj.set_key_string("code", "NetConnection.Connect.Success");
	rspMsg.infoObj.set_key_string("description", "Connection succeeded.");
	rspMsg.infoObj.set_key_number("objectEncoding", 0);
	SendRtmpMsg(evt->GetSocket(), &rspMsg, m_inChunkSize);

	// RtmpOnBwDone onBwDoneMsg;
	// SendRtmpMsg(evt->GetSocket(), &onBwDoneMsg, m_inChunkSize);
}

void MsRtmpHandler::ProcessCreateStream(RtmpHeader &header, shared_ptr<MsEvent> evt) {
	auto req = make_unique<RtmpCreateStreamReq>(header);
	// Decode the createStream request
	if (req->decode() < 0) {
		MS_LOG_ERROR("Failed to decode createStream request");
		return;
	}

	MS_LOG_DEBUG("Processing transaction id: %.2f cmd:%s", req->transactionId,
	             req->cmdName.c_str());

	// Send _result for createStream
	double streamId = 0; // For simplicity, always return stream ID 0
	RtmpCreateStreamRsp rspMsg(req->transactionId, streamId);
	SendRtmpMsg(evt->GetSocket(), &rspMsg, m_inChunkSize);
}

void MsRtmpHandler::ProcessReleaseStream(RtmpHeader &header, shared_ptr<MsEvent> evt) {
	auto req = make_unique<RtmpFMLEStartReq>(header);
	// Decode the releaseStream request
	if (req->decode() < 0) {
		MS_LOG_ERROR("Failed to decode releaseStream request");
		return;
	}

	MS_LOG_DEBUG("Processing transaction id: %.2f cmd:%s for stream: %s", req->transactionId,
	             req->cmdName.c_str(), req->streamName.c_str());

	RtmpFMLEStartRsp rspMsg(req->transactionId);
	SendRtmpMsg(evt->GetSocket(), &rspMsg, m_inChunkSize);
}

void MsRtmpHandler::ProcessPublish(RtmpHeader &header, shared_ptr<MsEvent> evt) {
	auto req = make_unique<RtmpPublishReq>(header);
	// Decode the publish request
	if (req->decode() < 0) {
		MS_LOG_ERROR("Failed to decode publish request");
		return;
	}

	MS_LOG_DEBUG("Processing transaction id: %.2f cmd:%s for stream: %s type: %s",
	             req->transactionId, req->cmdName.c_str(), req->streamName.c_str(),
	             req->type.c_str());

	auto source = m_server->GetRtmpSource(req->streamName);
	if (source) {
		// send error response
		MS_LOG_ERROR("Stream name already exists: %s", req->streamName.c_str());
		RtmpOnStatusCall errorMsg(0);
		errorMsg.cmdName = RTMP_AMF0_COMMAND_ON_STATUS;
		errorMsg.dataObj.set_key_string("level", "error");
		errorMsg.dataObj.set_key_string("code", "NetStream.Publish.BadName");
		errorMsg.dataObj.set_key_string("description", "Stream name already exists.");
		SendRtmpMsg(evt->GetSocket(), &errorMsg, m_inChunkSize);
		return;
	}

	if (!m_rtmpSource) {
		m_rtmpSource = make_shared<MsRtmpSource>(req->streamName + "_rtmp");
		m_server->RegistSource(req->streamName, m_rtmpSource);
		m_rtmpSource->Work();
	}

	uint32_t streamId = 0;
	RtmpOnStatusCall onStatusMsg(streamId);
	onStatusMsg.cmdName = RTMP_AMF0_COMMAND_ON_FC_PUBLISH;
	onStatusMsg.dataObj.set_key_string("level", "status");
	onStatusMsg.dataObj.set_key_string("code", "NetStream.Publish.Start");
	onStatusMsg.dataObj.set_key_string("description", "Start publishing stream.");
	SendRtmpMsg(evt->GetSocket(), &onStatusMsg, m_inChunkSize);

	// RtmpStreamBegin streamBeginMsg(streamId);
	// SendRtmpMsg(evt->GetSocket(), &streamBeginMsg, m_inChunkSize);

	RtmpOnStatusCall publishStatusMsg(streamId);
	publishStatusMsg.cmdName = RTMP_AMF0_COMMAND_ON_STATUS;
	publishStatusMsg.dataObj.set_key_string("level", "status");
	publishStatusMsg.dataObj.set_key_string("code", "NetStream.Publish.Start");
	publishStatusMsg.dataObj.set_key_string("description", "Start publishing stream.");
	SendRtmpMsg(evt->GetSocket(), &publishStatusMsg, m_inChunkSize);
}

void MsRtmpHandler::ProcessFCUnpublish(RtmpHeader &header, shared_ptr<MsEvent> evt) {
	// Implementation of unpublish command processing
	auto req = make_unique<RtmpFMLEStartReq>(header);
	// Decode the unpublish request
	if (req->decode() < 0) {
		MS_LOG_ERROR("Failed to decode unpublish request");
		return;
	}
	MS_LOG_DEBUG("Processing transaction id: %.2f cmd:%s for stream: %s", req->transactionId,
	             req->cmdName.c_str(), req->streamName.c_str());

	uint32_t streamId = 0;

	RtmpOnStatusCall onFcunpublish(streamId);
	onFcunpublish.cmdName = RTMP_AMF0_COMMAND_ON_FC_UNPUBLISH;
	onFcunpublish.dataObj.set_key_string("level", "status");
	onFcunpublish.dataObj.set_key_string("code", "NetStream.Unpublish.Success");
	onFcunpublish.dataObj.set_key_string("description", "Stream unpublished successfully.");
	if (SendRtmpMsg(evt->GetSocket(), &onFcunpublish, m_inChunkSize) < 0) {
		MS_LOG_DEBUG("Failed to send onFCUnpublish message");
		return;
	}

	RtmpFMLEStartRsp rspMsg(req->transactionId);
	rspMsg.msgStreamId = streamId;
	if (SendRtmpMsg(evt->GetSocket(), &rspMsg, m_inChunkSize) < 0) {
		MS_LOG_DEBUG("Failed to send FMLEStart response");
		return;
	}

	RtmpOnStatusCall onStatusMsg(streamId);
	onStatusMsg.cmdName = RTMP_AMF0_COMMAND_ON_STATUS;
	onStatusMsg.dataObj.set_key_string("level", "status");
	onStatusMsg.dataObj.set_key_string("code", "NetStream.Unpublish.Success");
	onStatusMsg.dataObj.set_key_string("description", "Stream unpublished successfully.");
	if (SendRtmpMsg(evt->GetSocket(), &onStatusMsg, m_inChunkSize) < 0) {
		MS_LOG_DEBUG("Failed to send onStatus message");
		return;
	}
}
