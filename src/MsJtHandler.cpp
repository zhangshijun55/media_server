#include "MsJtHandler.h"
#include "MsJtServer.h"
#include "MsLog.h"
#include <cstring>

MsJtHandler::MsJtHandler(const shared_ptr<MsJtServer> &server)
    : m_server(server), m_bufSize(4096), m_bufOff(0) {
	m_bufPtr = make_unique<uint8_t[]>(m_bufSize);
}

MsJtHandler::~MsJtHandler() {}

void MsJtHandler::HandleRead(shared_ptr<MsEvent> evt) {
	MsSocket *sock = evt->GetSocket();
	int n = sock->Recv((char *)m_bufPtr.get() + m_bufOff, m_bufSize - m_bufOff);
	if (n <= 0) {
		MS_LOG_WARN("JT connection closed or error, fd=%d", sock->GetFd());
		return;
	}

	m_bufOff += n;
	this->ProcessBuffer(evt);
}

void MsJtHandler::HandleClose(shared_ptr<MsEvent> evt) {
	MsSocket *sock = evt->GetSocket();
	MS_LOG_INFO("JT connection closed, fd=%d, terminal=%s", sock->GetFd(), m_terminalId.c_str());
	// post delete socket event
	MsMsg msg;
	msg.m_msgID = MS_JT_SOCKET_CLOSE;
	msg.m_intVal = sock->GetFd();
	msg.m_strVal = m_terminalId;
	m_server->EnqueMsg(msg);

	m_server->DelEvent(evt);
}

void MsJtHandler::ProcessBuffer(shared_ptr<MsEvent> evt) {
	// JT/T 808 message format: 0x7e + header + body + checksum + 0x7e
	// Find complete messages in buffer
	while (m_bufOff > 0) {
		// Find start marker
		int start = -1;
		for (int i = 0; i < m_bufOff; i++) {
			if (m_bufPtr[i] == 0x7e) {
				start = i;
				break;
			}
		}

		if (start < 0) {
			// No start marker, discard all data
			m_bufOff = 0;
			break;
		}

		if (start > 0) {
			// Discard data before start marker
			memmove(m_bufPtr.get(), m_bufPtr.get() + start, m_bufOff - start);
			m_bufOff -= start;
		}

		// Find end marker (skip first 0x7e)
		int end = -1;
		for (int i = 1; i < m_bufOff; i++) {
			if (m_bufPtr[i] == 0x7e) {
				end = i;
				break;
			}
		}

		if (end < 0) {
			// No end marker yet, wait for more data
			break;
		}

		// Extract message (excluding start and end markers)
		vector<uint8_t> rawData;
		Unescape(m_bufPtr.get() + 1, end - 1, rawData);

		// Parse message
		JT808Message msg;
		if (ParseMessage(rawData, msg)) {
			if (m_terminalId.empty()) {
				m_terminalId = msg.header.terminalPhone;
			}
			m_server->HandleJtMessage(evt, msg);
		} else {
			MS_LOG_WARN("JT parse message failed");
		}

		// Remove processed message from buffer
		int consumed = end + 1;
		if (consumed < m_bufOff) {
			memmove(m_bufPtr.get(), m_bufPtr.get() + consumed, m_bufOff - consumed);
		}
		m_bufOff -= consumed;
	}

	// Expand buffer if needed
	if (m_bufOff >= m_bufSize - 1024) {
		int newSize = m_bufSize * 2;
		auto newBuf = make_unique<uint8_t[]>(newSize);
		memcpy(newBuf.get(), m_bufPtr.get(), m_bufOff);
		m_bufPtr = std::move(newBuf);
		m_bufSize = newSize;
		MS_LOG_DEBUG("JT buffer expanded to %d", m_bufSize);
	}
}

void MsJtHandler::Unescape(const uint8_t *data, int len, vector<uint8_t> &result) {
	result.reserve(len);

	for (int i = 0; i < len; i++) {
		if (data[i] == 0x7d && i + 1 < len) {
			if (data[i + 1] == 0x02) {
				result.push_back(0x7e);
				i++;
			} else if (data[i + 1] == 0x01) {
				result.push_back(0x7d);
				i++;
			} else {
				// this is highly unexpected, just push the byte
				MS_LOG_WARN("JT unexpected escape sequence: 0x7d 0x%02x", data[i + 1]);
				result.push_back(data[i]);
			}
		} else {
			result.push_back(data[i]);
		}
	}
}

// TODO: is this function used?
vector<uint8_t> MsJtHandler::Escape(const uint8_t *data, int len) {
	vector<uint8_t> result;
	result.reserve(len * 2);

	for (int i = 0; i < len; i++) {
		if (data[i] == 0x7e) {
			result.push_back(0x7d);
			result.push_back(0x02);
		} else if (data[i] == 0x7d) {
			result.push_back(0x7d);
			result.push_back(0x01);
		} else {
			result.push_back(data[i]);
		}
	}

	return result;
}

uint8_t MsJtHandler::CalcChecksum(const uint8_t *data, int len) {
	uint8_t checksum = 0;
	for (int i = 0; i < len; i++) {
		checksum ^= data[i];
	}
	return checksum;
}

bool MsJtHandler::ParseHeader(const vector<uint8_t> &data, JT808Header &header, int &headerLen) {
	// Message ID (2 bytes, big-endian)
	header.msgId = (data[0] << 8) | data[1];

	// Message body attributes (2 bytes)
	header.msgBodyAttr = (data[2] << 8) | data[3];
	header.protocolVersion = data[4];

	// Terminal phone number (BCD, 10 bytes)
	char phone[21] = {0};
	for (int i = 0; i < 10; i++) {
		sprintf(phone + i * 2, "%02X", data[5 + i]);
	}
	header.terminalPhone = phone;

	// Message serial number (2 bytes)
	header.msgSerialNo = (data[15] << 8) | data[16];

	headerLen = 17;

	// Check if message is split
	if (header.isSplit()) {
		if (data.size() < 17 + 4) {
			return false;
		}
		header.totalPackets = (data[17] << 8) | data[18];
		header.packetNo = (data[19] << 8) | data[20];
		headerLen = 21;
	} else {
		header.totalPackets = 0;
		header.packetNo = 0;
	}

	return true;
}

bool MsJtHandler::ParseMessage(const vector<uint8_t> &data, JT808Message &msg) {
	if (data.size() < 18) { // Minimum: 17 header + 1 checksum
		MS_LOG_WARN("JT message too short: %zu", data.size());
		return false;
	}

	int headerLen;
	if (!ParseHeader(data, msg.header, headerLen)) {
		MS_LOG_WARN("JT parse header failed");
		return false;
	}

	// Verify checksum
	msg.checksum = data.back();
	uint8_t calcChecksum = CalcChecksum(data.data(), data.size() - 1);
	if (msg.checksum != calcChecksum) {
		MS_LOG_WARN("JT checksum mismatch: expected 0x%02x, got 0x%02x", calcChecksum,
		            msg.checksum);
		return false;
	}

	// Extract body
	int bodyLen = msg.header.bodyLength();
	if (headerLen + bodyLen + 1 > (int)data.size()) {
		MS_LOG_WARN("JT body length mismatch");
		return false;
	}

	msg.body.assign(data.begin() + headerLen, data.begin() + headerLen + bodyLen);

	MS_LOG_DEBUG("JT parsed message: id=0x%04x, phone=%s, serial=%d, bodyLen=%d", msg.header.msgId,
	             msg.header.terminalPhone.c_str(), msg.header.msgSerialNo, bodyLen);

	return true;
}
