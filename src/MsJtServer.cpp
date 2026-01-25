#include "MsJtServer.h"
#include "MsCommon.h"
#include "MsConfig.h"
#include "MsHttpMsg.h"
#include "MsJtHandler.h"
#include "MsLog.h"
#include "MsMsgDef.h"
#include <memory>
#include <nlohmann/json.hpp>
#include <thread>

///////////////////////////// MsJtServer /////////////////////////////

MsJtServer::MsJtServer(int type, int id) : MsReactor(type, id), m_serialNo(0) {}

void MsJtServer::Run() {
	this->RegistToManager();

	// test data
	shared_ptr<STerminalInfo> terminalInfo = make_shared<STerminalInfo>();
	terminalInfo->m_terminalId = "13800138000";
	terminalInfo->m_plate = "粤A12345";
	terminalInfo->m_authenticated = true;
	terminalInfo->m_avAttr = make_shared<SJtStreamInfo>();
	terminalInfo->m_avAttr->m_videoCodec = 98; // H.264
	terminalInfo->m_avAttr->m_audioCodec = 19; // AAC
	terminalInfo->m_channelInfo = make_shared<SJtChannelInfo>();
	terminalInfo->m_channelInfo->m_avChannelTotal = 1;
	terminalInfo->m_channelInfo->m_audioChannelTotal = 0;
	terminalInfo->m_channelInfo->m_videoChannelTotal = 0;
	terminalInfo->m_channelInfo->m_channels.push_back({1, 1, 0, 0});

	m_terminals.emplace(terminalInfo->m_terminalId, terminalInfo);

	// Detach worker thread
	thread(&MsJtServer::Wait, this).detach();
}

void MsJtServer::HandleMsg(MsMsg &msg) {
	// Handle internal messages if needed
	switch (msg.m_msgID) {
	case MS_SOCK_TRANSFER_MSG: {
		auto jtMsg = std::any_cast<shared_ptr<SSockTransferMsg>>(msg.m_any);
		// Process jtMsg as needed
		shared_ptr<MsJtHandler> handler =
		    make_shared<MsJtHandler>(dynamic_pointer_cast<MsJtServer>(shared_from_this()));
		shared_ptr<MsEvent> event =
		    make_shared<MsEvent>(jtMsg->sock, MS_FD_READ | MS_FD_CLOSE, handler);
		this->AddEvent(event);

		// copy data to handler buffer and process
		if (jtMsg->data.size() > handler->m_bufSize) {
			handler->m_bufPtr = make_unique<uint8_t[]>(jtMsg->data.size());
			handler->m_bufSize = jtMsg->data.size();
		}

		memcpy(handler->m_bufPtr.get(), jtMsg->data.data(), jtMsg->data.size());
		handler->m_bufOff = jtMsg->data.size();
		handler->ProcessBuffer(event);

	} break;

	case MS_JT_SOCKET_CLOSE: {
		int fd = msg.m_intVal;
		// Find and remove terminal info by socket fd
		for (auto it = m_terminals.begin(); it != m_terminals.end();) {
			shared_ptr<STerminalInfo> &terminalInfo = it->second;
			if (terminalInfo->m_sock && terminalInfo->m_sock->GetFd() == fd) {
				MS_LOG_INFO("JT terminal socket closed: %s", terminalInfo->m_terminalId.c_str());
				it = m_terminals.erase(it);
			} else {
				++it;
			}
		}

	} break;

	case MS_JT_GET_STREAM_INFO: {
		MsMsg rspMsg;
		rspMsg.m_msgID = MS_JT_GET_STREAM_INFO;
		rspMsg.m_dstType = msg.m_srcType;
		rspMsg.m_dstID = msg.m_srcID;

		auto it = m_terminals.find(msg.m_strVal);
		if (it != m_terminals.end()) {
			shared_ptr<STerminalInfo> &terminalInfo = it->second;
			if (terminalInfo->m_avAttr) {
				rspMsg.m_intVal = 0; // indicate success
				rspMsg.m_any = terminalInfo->m_avAttr;
			} else {
				MS_LOG_ERROR("JT terminal %s has no AV attributes", msg.m_strVal.c_str());
				rspMsg.m_intVal = -1; // indicate failure
			}
		} else {
			MS_LOG_ERROR("JT terminal %s not found for stream info request", msg.m_strVal.c_str());
			rspMsg.m_intVal = -1; // indicate failure
		}
		this->PostMsg(rspMsg);
	} break;

	case MS_JT_START_STREAM:
		this->RequestLiveStream(msg);
		break;

	case MS_JT_STOP_STREAM:
		this->StopLiveStream(msg);
		break;

	case MS_HTTP_TRANSFER_MSG: {
		auto httpMsg = std::any_cast<shared_ptr<SHttpTransferMsg>>(msg.m_any);
		this->HandleHttpMsg(httpMsg);
	} break;

	case MS_JT_REQ_TIMEOUT: {
		string terminalPhone = msg.m_strVal;
		int reqMsgId = msg.m_intVal;

		auto it = m_terminals.find(terminalPhone);
		if (it != m_terminals.end()) {
			shared_ptr<STerminalInfo> &terminalInfo = it->second;
			MsMsg &reqMsg = terminalInfo->m_reqMsg;
			MS_LOG_WARN("JT terminal %s msg:%d timeout", terminalPhone.c_str(), reqMsg.m_msgID);

			MsMsg rsp;
			rsp.m_msgID = reqMsg.m_msgID;
			rsp.m_srcType = reqMsg.m_srcType;
			rsp.m_srcID = reqMsg.m_srcID;
			rsp.m_intVal = -1; // indicate failure
			this->PostMsg(rsp);

			terminalInfo->m_waitTimerID = -1;
			terminalInfo->m_waitSeq = UINT16_MAX;
		}
	} break;

	default:
		MsReactor::HandleMsg(msg);
		break;
	}
}

void MsJtServer::HandleJtMessage(shared_ptr<MsEvent> evt, JT808Message &msg) {
	MS_LOG_INFO("JT received message: id=0x%04x from %s", msg.header.msgId,
	            msg.header.terminalPhone.c_str());

	// find map, update socket if different
	shared_ptr<STerminalInfo> terminalInfo;
	auto it = m_terminals.find(msg.header.terminalPhone);
	if (it != m_terminals.end()) {
		terminalInfo = it->second;
		if (terminalInfo->m_sock != evt->GetSharedSocket()) {
			MS_LOG_INFO("JT terminal %s socket updated", msg.header.terminalPhone.c_str());
			terminalInfo->m_sock = evt->GetSharedSocket();
		}
	} else {
		if (msg.header.msgId != JT_TERMINAL_REGISTER) {
			MS_LOG_WARN("JT message from unregistered terminal: %s",
			            msg.header.terminalPhone.c_str());
			// Send common response with result=2 (not registered)
			SendCommonResponse(evt->GetSharedSocket(), msg.header.terminalPhone,
			                   msg.header.msgSerialNo, msg.header.msgId, 2);
			return;
		}
	}

	switch (msg.header.msgId) {
	case JT_TERMINAL_REGISTER:
		OnTerminalRegister(evt, msg, terminalInfo);
		break;
	case JT_TERMINAL_AUTH:
		OnTerminalAuth(evt, msg, terminalInfo);
		break;
	case JT_TERMINAL_HEARTBEAT:
		OnTerminalHeartbeat(evt, msg, terminalInfo);
		break;
	case JT_LOCATION_REPORT:
		OnLocationReport(evt, msg, terminalInfo);
		break;
	case JT_TERMINAL_UNREGISTER:
		OnTerminalUnregister(evt, msg, terminalInfo);
		break;
	case JT_AV_ATTR_QUERY_RSP:
		OnAvAttrQueryRsp(evt, msg, terminalInfo);
		break;
	case JT_PARAM_QUERY_RSP:
		OnParamQueryRsp(evt, msg, terminalInfo);
		break;
	case JT_TERMINAL_COMMON_RSP:
		OnTerminalCommonRsp(evt, msg, terminalInfo);
		break;

	default:
		MS_LOG_WARN("JT unsupported message id: 0x%04x", msg.header.msgId);
		// Send common response with result=3 (unsupported)
		SendCommonResponse(evt->GetSharedSocket(), msg.header.terminalPhone, msg.header.msgSerialNo,
		                   msg.header.msgId, 3);
		break;
	}
}

void MsJtServer::OnTerminalRegister(shared_ptr<MsEvent> evt, JT808Message &msg,
                                    shared_ptr<STerminalInfo> &terminalInfo) {
	MS_LOG_INFO("JT terminal register: phone=%s", msg.header.terminalPhone.c_str());

	// Parse registration data
	// Body format: province(2) + city(2) + manufacturer(11) + model(30) + terminalId(30) +
	// plateColor(1) + plate(variable)

	if (msg.body.size() < 76) {
		MS_LOG_WARN("JT register body too short");
		return;
	}

	string plate = string(msg.body.begin() + 76, msg.body.end());
	string authCode = GenRandStr(16);

	if (terminalInfo) {
		MS_LOG_WARN("JT terminal already registered: %s, update auth code: %s",
		            msg.header.terminalPhone.c_str(), authCode.c_str());
		// Update auth code
		terminalInfo->m_authCode = authCode;
		terminalInfo->m_authenticated = false;
		terminalInfo->m_plate = plate;

	} else {
		MS_LOG_INFO("JT new terminal registered: %s", msg.header.terminalPhone.c_str());
		// Store terminal info
		auto terminalInfo = make_shared<STerminalInfo>();
		terminalInfo->m_sock = evt->GetSharedSocket();
		terminalInfo->m_terminalId = msg.header.terminalPhone;
		terminalInfo->m_authCode = authCode;
		terminalInfo->m_plate = plate;
		terminalInfo->m_lastHeartbeat = std::chrono::steady_clock::now();
		m_terminals[msg.header.terminalPhone] = terminalInfo;
	}

	// Send registration response
	// Response format: serial(2) + result(1) + authCode(variable)
	vector<uint8_t> body;
	body.push_back((msg.header.msgSerialNo >> 8) & 0xFF);
	body.push_back(msg.header.msgSerialNo & 0xFF);
	body.push_back(0x00); // Result: 0=success
	body.insert(body.end(), authCode.begin(), authCode.end());

	SendMessage(evt->GetSharedSocket(), JT_TERMINAL_REGISTER_RSP, msg.header.terminalPhone, body);
}

void MsJtServer::OnTerminalAuth(shared_ptr<MsEvent> evt, JT808Message &msg,
                                shared_ptr<STerminalInfo> &terminalInfo) {
	MS_LOG_INFO("JT terminal auth: phone=%s", msg.header.terminalPhone.c_str());

	// Body contains auth code
	string authCode(msg.body.begin(), msg.body.end());
	MS_LOG_DEBUG("JT auth code: %s", authCode.c_str());

	// Verify auth code (simple verification)
	bool success = authCode == terminalInfo->m_authCode;

	// Send common response
	SendCommonResponse(evt->GetSharedSocket(), msg.header.terminalPhone, msg.header.msgSerialNo,
	                   msg.header.msgId, success ? 0 : 1); // 0=success, 1=fail

	if (success) {
		terminalInfo->m_authenticated = true;
		MS_LOG_INFO("JT terminal authenticated: %s", msg.header.terminalPhone.c_str());

		this->CheckStreamInfo(terminalInfo);
	}
}

void MsJtServer::OnTerminalHeartbeat(shared_ptr<MsEvent> evt, JT808Message &msg,
                                     shared_ptr<STerminalInfo> &terminalInfo) {
	MS_LOG_DEBUG("JT heartbeat from %s", msg.header.terminalPhone.c_str());

	terminalInfo->m_lastHeartbeat = std::chrono::steady_clock::now();

	// Send common response
	SendCommonResponse(evt->GetSharedSocket(), msg.header.terminalPhone, msg.header.msgSerialNo,
	                   msg.header.msgId, 0);

	this->CheckStreamInfo(terminalInfo);
}

void MsJtServer::OnLocationReport(shared_ptr<MsEvent> evt, JT808Message &msg,
                                  shared_ptr<STerminalInfo> &terminalInfo) {
	// Parse location data
	if (!ParseLocation(msg.body, msg.location)) {
		MS_LOG_WARN("JT parse location failed");
		return;
	}

	// Convert coordinates
	double lat = msg.location.latitude / 1000000.0;
	double lng = msg.location.longitude / 1000000.0;
	double speed = msg.location.speed / 10.0;

	MS_LOG_INFO("JT location from %s: lat=%.6f, lng=%.6f, speed=%.1fkm/h, time=%s",
	            msg.header.terminalPhone.c_str(), lat, lng, speed, msg.location.timestamp.c_str());

	// Send common response
	SendCommonResponse(evt->GetSharedSocket(), msg.header.terminalPhone, msg.header.msgSerialNo,
	                   msg.header.msgId, 0);

	// TODO: Store location to database or forward to other services
}

void MsJtServer::OnTerminalUnregister(shared_ptr<MsEvent> evt, JT808Message &msg,
                                      shared_ptr<STerminalInfo> &terminalInfo) {
	MS_LOG_INFO("JT terminal unregister: phone=%s", msg.header.terminalPhone.c_str());

	// Send common response
	SendCommonResponse(evt->GetSharedSocket(), msg.header.terminalPhone, msg.header.msgSerialNo,
	                   msg.header.msgId, 0);

	m_terminals.erase(msg.header.terminalPhone);
	MS_LOG_INFO("JT terminal info removed: %s", msg.header.terminalPhone.c_str());
}

void MsJtServer::OnAvAttrQueryRsp(shared_ptr<MsEvent> evt, JT808Message &msg,
                                  shared_ptr<STerminalInfo> &terminalInfo) {
	if (msg.body.size() < 10) {
		MS_LOG_WARN("JT AV attribute query response body too short: %zu", msg.body.size());
		return;
	}

	auto &avAttr = terminalInfo->m_avAttr;
	if (!avAttr) {
		avAttr = make_shared<SJtStreamInfo>();
	}
	avAttr->m_audioCodec = msg.body[0];
	avAttr->m_audioChannels = msg.body[1];
	avAttr->m_audioSampleRate = msg.body[2];
	avAttr->m_audioBits = msg.body[3];
	avAttr->m_audioFrameLen = (msg.body[4] << 8) | msg.body[5];
	avAttr->m_audioOutput = msg.body[6];
	avAttr->m_videoCodec = msg.body[7];
	avAttr->m_maxAudioChannels = msg.body[8];
	avAttr->m_maxVideoChannels = msg.body[9];
	MS_LOG_INFO("JT AV attributes: phone=%s, audioCodec=%d, audioChannels=%d, audioSampleRate=%d, "
	            "audioBits=%d, videoCodec=%d, maxAudioChannels=%d, maxVideoChannels=%d",
	            msg.header.terminalPhone.c_str(), avAttr->m_audioCodec, avAttr->m_audioChannels,
	            avAttr->m_audioSampleRate, avAttr->m_audioBits, avAttr->m_videoCodec,
	            avAttr->m_maxAudioChannels, avAttr->m_maxVideoChannels);

	this->CheckStreamInfo(terminalInfo);
}

void MsJtServer::OnParamQueryRsp(shared_ptr<MsEvent> evt, JT808Message &msg,
                                 shared_ptr<STerminalInfo> &terminalInfo) {
	// Body: Serial No (2B), Count (1B), [Param ID (4B), Param Len (1B), Param Val (N)]...
	if (msg.body.size() < 3) {
		MS_LOG_WARN("JT param query rsp too short: %zu", msg.body.size());
		return;
	}

	uint16_t reqSerial = ((uint16_t)msg.body[0] << 8) | msg.body[1];
	uint8_t count = msg.body[2];
	int offset = 3;

	auto &paramRsp = terminalInfo->m_channelInfo;
	if (!paramRsp) {
		paramRsp = make_shared<SJtChannelInfo>();
	}
	paramRsp->m_channels.clear();

	for (int i = 0; i < count; ++i) {
		if (offset + 5 > (int)msg.body.size())
			break;

		uint32_t paramId = ((uint32_t)msg.body[offset] << 24) |
		                   ((uint32_t)msg.body[offset + 1] << 16) |
		                   ((uint32_t)msg.body[offset + 2] << 8) | msg.body[offset + 3];
		uint8_t paramLen = msg.body[offset + 4];
		offset += 5;

		if (offset + paramLen > (int)msg.body.size())
			break;

		if (paramId == 0x00000076) {
			// AV Channel Parameters
			if (paramLen >= 4) {
				paramRsp->m_avChannelTotal = msg.body[offset];
				paramRsp->m_audioChannelTotal = msg.body[offset + 1];
				paramRsp->m_videoChannelTotal = msg.body[offset + 2];

				int totalChannels = paramRsp->m_audioChannelTotal + paramRsp->m_videoChannelTotal +
				                    paramRsp->m_avChannelTotal;

				for (int ch = 0; ch < totalChannels; ++ch) {
					if (offset + 3 + ch * 4 + 4 > (int)msg.body.size())
						break;

					SJtChannelItem channelInfo;
					channelInfo.m_phyChanId = msg.body[offset + 3 + ch * 4];
					channelInfo.m_logicChanId = msg.body[offset + 3 + ch * 4 + 1];
					channelInfo.m_chanType = msg.body[offset + 3 + ch * 4 + 2];
					channelInfo.m_ptz = msg.body[offset + 3 + ch * 4 + 3];
					paramRsp->m_channels.push_back(channelInfo);
				}

				MS_LOG_INFO("JT terminal %s AV channel param: total=%d, audio=%d, video=%d",
				            terminalInfo->m_terminalId.c_str(), paramRsp->m_avChannelTotal,
				            paramRsp->m_audioChannelTotal, paramRsp->m_videoChannelTotal);
			}
		}

		offset += paramLen;
	}
}

void MsJtServer::OnTerminalCommonRsp(shared_ptr<MsEvent> evt, JT808Message &msg,
                                     shared_ptr<STerminalInfo> &terminalInfo) {
	if (msg.body.size() < 5) {
		MS_LOG_WARN("JT terminal common response body too short: %zu", msg.body.size());
		return;
	}

	uint16_t respSerialNo = (msg.body[0] << 8) | msg.body[1];
	uint16_t respMsgId = (msg.body[2] << 8) | msg.body[3];
	uint8_t result = msg.body[4];

	MS_LOG_INFO(
	    "JT terminal common response: phone=%s, respSerial=0x%04x, respMsgId=0x%04x, result=%d",
	    msg.header.terminalPhone.c_str(), respSerialNo, respMsgId, result);

	if (respMsgId == JT_LIVE_STREAM_REQ && terminalInfo->m_waitTimerID != -1 &&
	    respSerialNo == terminalInfo->m_waitSeq) {
		MsMsg rsp;
		rsp.m_msgID = terminalInfo->m_reqMsg.m_msgID;
		rsp.m_srcType = terminalInfo->m_reqMsg.m_srcType;
		rsp.m_srcID = terminalInfo->m_reqMsg.m_srcID;
		rsp.m_intVal = (result == 0) ? 0 : -1; // success or failure
		this->PostMsg(rsp);

		this->DelTimer(terminalInfo->m_waitTimerID);
		terminalInfo->m_waitTimerID = -1;
		terminalInfo->m_waitSeq = UINT16_MAX;
	}
}

bool MsJtServer::ParseLocation(const vector<uint8_t> &body, JT808Location &loc) {
	if (body.size() < 28) {
		return false;
	}

	// Alarm flags (4 bytes)
	loc.alarmFlag = (body[0] << 24) | (body[1] << 16) | (body[2] << 8) | body[3];

	// Status (4 bytes)
	loc.status = (body[4] << 24) | (body[5] << 16) | (body[6] << 8) | body[7];

	// Latitude (4 bytes)
	loc.latitude = (body[8] << 24) | (body[9] << 16) | (body[10] << 8) | body[11];

	// Longitude (4 bytes)
	loc.longitude = (body[12] << 24) | (body[13] << 16) | (body[14] << 8) | body[15];

	// Altitude (2 bytes)
	loc.altitude = (body[16] << 8) | body[17];

	// Speed (2 bytes)
	loc.speed = (body[18] << 8) | body[19];

	// Direction (2 bytes)
	loc.direction = (body[20] << 8) | body[21];

	// Timestamp (BCD, 6 bytes: YYMMDDhhmmss)
	loc.timestamp = BcdToString(&body[22], 6);

	return true;
}

string MsJtServer::BcdToString(const uint8_t *bcd, int len) {
	string result;
	result.reserve(len * 2);

	for (int i = 0; i < len; i++) {
		char buf[3];
		sprintf(buf, "%02X", bcd[i]);
		result += buf;
	}

	return result;
}

vector<uint8_t> MsJtServer::StringToBcd(const string &str, int len) {
	vector<uint8_t> result(len, 0);

	int strLen = str.length();
	int start = len * 2 - strLen;

	for (int i = 0; i < strLen; i += 2) {
		int idx = (start + i) / 2;
		if (idx >= 0 && idx < len) {
			char buf[3] = {0};
			buf[0] = str[i];
			buf[1] = (i + 1 < strLen) ? str[i + 1] : '0';
			result[idx] = (uint8_t)strtol(buf, nullptr, 16);
		}
	}

	return result;
}

void MsJtServer::SendMessage(shared_ptr<MsSocket> sock, uint16_t msgId, const string &terminalPhone,
                             const vector<uint8_t> &body) {
	vector<uint8_t> packet;

	// Message ID
	packet.push_back((msgId >> 8) & 0xFF);
	packet.push_back(msgId & 0xFF);

	// Message body attributes (body length)
	uint16_t attr = (body.size() & 0x03FF) | (1 << 14); // Set version identifier bit
	packet.push_back((attr >> 8) & 0xFF);
	packet.push_back(attr & 0xFF);

	// version uint8_t
	packet.push_back(0x01); // Protocol version 1

	// Terminal phone (BCD, 10 bytes)
	vector<uint8_t> phone = StringToBcd(terminalPhone, 10);
	packet.insert(packet.end(), phone.begin(), phone.end());

	// Message serial number
	packet.push_back((m_serialNo >> 8) & 0xFF);
	packet.push_back(m_serialNo & 0xFF);
	m_serialNo++;

	// Body
	packet.insert(packet.end(), body.begin(), body.end());

	// Calculate checksum
	uint8_t checksum = 0;
	for (auto b : packet) {
		checksum ^= b;
	}
	packet.push_back(checksum);

	// Escape special characters
	vector<uint8_t> escaped;
	escaped.push_back(0x7e);

	for (size_t i = 0; i < packet.size(); i++) {
		if (packet[i] == 0x7e) {
			escaped.push_back(0x7d);
			escaped.push_back(0x02);
		} else if (packet[i] == 0x7d) {
			escaped.push_back(0x7d);
			escaped.push_back(0x01);
		} else {
			escaped.push_back(packet[i]);
		}
	}

	escaped.push_back(0x7e);

	// Send
	sock->Send((const char *)escaped.data(), escaped.size());

	MS_LOG_DEBUG("JT sent message: msgId=0x%04x, len=%zu", msgId, escaped.size());
}

void MsJtServer::SendCommonResponse(shared_ptr<MsSocket> sock, const string &terminalPhone,
                                    uint16_t respSerialNo, uint16_t respMsgId, uint8_t result) {
	// Platform common response body: serial(2) + msgId(2) + result(1)
	vector<uint8_t> body;
	body.push_back((respSerialNo >> 8) & 0xFF);
	body.push_back(respSerialNo & 0xFF);
	body.push_back((respMsgId >> 8) & 0xFF);
	body.push_back(respMsgId & 0xFF);
	body.push_back(result);

	SendMessage(sock, JT_PLATFORM_COMMON_RSP, terminalPhone, body);
}

void MsJtServer::RequestLiveStream(MsMsg &msg) {
	string &terminalPhone = msg.m_strVal;
	shared_ptr<SJtStartStreamReq> streamReq =
	    std::any_cast<shared_ptr<SJtStartStreamReq>>(msg.m_any);
	string ip = streamReq->m_ip;
	uint16_t tcpPort = streamReq->m_tcpPort;
	uint16_t udpPort = streamReq->m_udpPort;
	uint8_t channel = streamReq->m_channel;
	uint8_t dataType = streamReq->m_dataType;
	uint8_t streamType = streamReq->m_streamType;

	MsMsg rsp;
	rsp.m_msgID = MS_JT_START_STREAM;
	rsp.m_srcType = msg.m_srcType;
	rsp.m_srcID = msg.m_srcID;

	auto it = m_terminals.find(msg.m_strVal);
	if (it != m_terminals.end()) {
		shared_ptr<STerminalInfo> terminalInfo = it->second;
		if (terminalInfo->m_authenticated && terminalInfo->m_sock) {
			if (terminalInfo->m_waitTimerID != -1) {
				// already waiting for previous request
				MS_LOG_INFO("JT terminal %s has request pending", terminalPhone.c_str());
				rsp.m_intVal = -1; // indicate failure
				this->PostMsg(rsp);
				return;
			}
			terminalInfo->m_reqMsg = msg;

			// Construct body
			vector<uint8_t> body;

			// Server IP length (1 byte)
			uint8_t ipLen = ip.length();
			body.push_back(ipLen);

			// Server IP (n bytes)
			body.insert(body.end(), ip.begin(), ip.end());

			// TCP port (2 bytes)
			body.push_back((tcpPort >> 8) & 0xFF);
			body.push_back(tcpPort & 0xFF);

			// UDP port (2 bytes)
			body.push_back((udpPort >> 8) & 0xFF);
			body.push_back(udpPort & 0xFF);

			// Logical Channel No (1 byte)
			body.push_back(channel);

			// Data Type (1 byte)
			body.push_back(dataType);

			// Stream Type (1 byte)
			body.push_back(streamType);

			SendMessage(terminalInfo->m_sock, JT_LIVE_STREAM_REQ, terminalPhone, body);
			MS_LOG_INFO(
			    "JT sent live stream request to %s: ip=%s, tcp=%d, udp=%d, channel=%d, type=%d",
			    terminalPhone.c_str(), ip.c_str(), tcpPort, udpPort, channel, dataType);

			MsMsg toMsg;
			toMsg.m_msgID = MS_JT_REQ_TIMEOUT;
			toMsg.m_strVal = terminalPhone;
			toMsg.m_intVal = JT_LIVE_STREAM_REQ;
			terminalInfo->m_waitTimerID = this->AddTimer(toMsg, 10);
			terminalInfo->m_waitSeq = m_serialNo - 1;

		} else {
			MS_LOG_WARN("JT terminal %s not authenticated or socket invalid",
			            terminalPhone.c_str());
			rsp.m_intVal = -1; // indicate failure
			this->PostMsg(rsp);
		}
	} else {
		MS_LOG_WARN("JT terminal %s not found for live stream request", terminalPhone.c_str());
		rsp.m_intVal = -1; // indicate failure
		this->PostMsg(rsp);
	}
}

void MsJtServer::StopLiveStream(MsMsg &msg) {
	string &terminalPhone = msg.m_strVal;
	auto it = m_terminals.find(terminalPhone);
	if (it != m_terminals.end()) {
		shared_ptr<STerminalInfo> terminalInfo = it->second;
		if (terminalInfo->m_authenticated && terminalInfo->m_sock) {
			int channel = msg.m_intVal;

			// JT/T 1078-2016 0x9102
			vector<uint8_t> body;
			body.push_back(channel); // Logical Channel No
			body.push_back(0);       // Control: 0-Close A/V
			body.push_back(0);       // Close Type: 0-Close A/V
			body.push_back(0);       // Stream Type: 0-Main Stream

			SendMessage(terminalInfo->m_sock, JT_LIVE_STREAM_CONTROL, terminalPhone, body);
			MS_LOG_INFO("JT sent stop live stream to %s: channel=%d", terminalPhone.c_str(),
			            channel);
		} else {
			MS_LOG_WARN("JT terminal %s not authenticated or socket invalid",
			            terminalPhone.c_str());
		}
	} else {
		MS_LOG_WARN("JT terminal %s not found for stop stream", terminalPhone.c_str());
	}
}

void MsJtServer::CheckStreamInfo(shared_ptr<STerminalInfo> &terminalInfo) {
	if (terminalInfo->m_avAttr == nullptr) {
		// Query AV attributes if not available
		SendMessage(terminalInfo->m_sock, JT_AV_ATTR_QUERY, terminalInfo->m_terminalId, {});
		MS_LOG_INFO("JT sent AV attribute query to %s", terminalInfo->m_terminalId.c_str());
	} else if (terminalInfo->m_channelInfo == nullptr) {
		this->QueryAvChannel(terminalInfo);
	}
}

void MsJtServer::QueryAvChannel(shared_ptr<STerminalInfo> &terminalInfo) {
	// JT/T 808 0x8106 Query Specified Parameters
	vector<uint8_t> body;
	body.push_back(1); // Parameter count = 1
	// Parameter ID 0x00000076 (AV Channel Parameters)
	body.push_back(0x00);
	body.push_back(0x00);
	body.push_back(0x00);
	body.push_back(0x76);

	SendMessage(terminalInfo->m_sock, JT_PARAM_QUERY, terminalInfo->m_terminalId, body);
	MS_LOG_INFO("JT sent AV channel param query to %s", terminalInfo->m_terminalId.c_str());
}

void MsJtServer::HandleHttpMsg(shared_ptr<SHttpTransferMsg> rtcMsg) {
	auto &httpMsg = rtcMsg->httpMsg;
	if (httpMsg.m_uri.find("/jt/terminal/url") == 0) {
		string terminalId, channelId, streamType;
		uint8_t nChannelId = 1;
		uint8_t nStreamType = 0;
		GetParam("terminalId", terminalId, httpMsg.m_uri);
		GetParam("logicChanId", channelId, httpMsg.m_uri);
		GetParam("streamType", streamType, httpMsg.m_uri);

		if (terminalId.empty()) {
			MsHttpMsg rsp;
			rsp.m_version = httpMsg.m_version;
			rsp.m_status = "400";
			rsp.m_reason = "Bad Request";
			SendHttpRspEx(rtcMsg->sock.get(), rsp);
			return;
		}

		if (!channelId.empty()) {
			nChannelId = static_cast<uint8_t>(std::stoi(channelId));
		}
		if (!streamType.empty()) {
			nStreamType = static_cast<uint8_t>(std::stoi(streamType));
		}

		auto it = m_terminals.find(terminalId);
		nlohmann::json j;
		if (it != m_terminals.end()) {
			shared_ptr<STerminalInfo> &terminalInfo = it->second;
			if (terminalInfo->m_avAttr != nullptr && terminalInfo->m_channelInfo != nullptr) {
				bool chanFound = false;
				for (auto &c : terminalInfo->m_channelInfo->m_channels) {
					if (c.m_logicChanId == nChannelId) {
						chanFound = true;
						break;
					}
				}

				if (!chanFound) {
					j["code"] = 3;
					j["msg"] = "channel not found";
				} else {
					string streamId = terminalId + "_" + std::to_string(nChannelId) + "_" +
					                  std::to_string(nStreamType) + "_jt";

#if ENABLE_HTTPS
					string protocol = "https://";
#else
					string protocol = "http://";
#endif

					string httpIp = MsConfig::Instance()->GetConfigStr("localBindIP");
					int httpPort = MsConfig::Instance()->GetConfigInt("httpPort");
					nlohmann::json re;
					char bb[512];

					sprintf(bb, "%s%s:%d/live/%s.flv", protocol.c_str(), httpIp.c_str(), httpPort,
					        streamId.c_str());
					re["httpFlvUrl"] = bb;

					sprintf(bb, "%s%s:%d/live/%s.ts", protocol.c_str(), httpIp.c_str(), httpPort,
					        streamId.c_str());
					re["httpTsUrl"] = bb;

					sprintf(bb, "%s%s:%d/rtc/whep/%s", protocol.c_str(), httpIp.c_str(), httpPort,
					        streamId.c_str());
					re["rtcUrl"] = bb;

					sprintf(bb, "rtsp://%s:%d/live/%s", httpIp.c_str(), httpPort, streamId.c_str());
					re["rtspUrl"] = bb;

					j["code"] = 0;
					j["msg"] = "success";
					j["result"] = re;
				}

			} else {
				j["code"] = 2;
				j["msg"] = "terminal stream info not ready";
			}
		} else {
			j["code"] = 1;
			j["msg"] = "terminal not found";
		}

		SendHttpRspEx(rtcMsg->sock.get(), j.dump());

	} else if (httpMsg.m_uri.find("/jt/terminal") == 0) {
		nlohmann::json j, ts = nlohmann::json::array();
		for (auto &pair : m_terminals) {
			shared_ptr<STerminalInfo> &info = pair.second;
			nlohmann::json item;

			item["terminalId"] = info->m_terminalId;
			item["plate"] = info->m_plate;
			item["auth"] = info->m_authenticated;

			if (info->m_lastHeartbeat.time_since_epoch().count() > 0) {
				auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
				    info->m_lastHeartbeat.time_since_epoch());
				std::time_t t = ms.count() / 1000;
				char buf[32];
				std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
				item["lastHeartbeat"] = buf;
			} else {
				item["lastHeartbeat"] = nullptr;
			}

			// av attributes
			if (info->m_avAttr) {
				item["audioCodec"] = info->m_avAttr->m_audioCodec;
				item["audioChannels"] = info->m_avAttr->m_audioChannels;
				item["audioSampleRate"] = info->m_avAttr->m_audioSampleRate;
				item["videoCodec"] = info->m_avAttr->m_videoCodec;
			}

			// channels
			if (info->m_channelInfo) {
				item["avChannelTotal"] = info->m_channelInfo->m_avChannelTotal;
				item["audioChannelTotal"] = info->m_channelInfo->m_audioChannelTotal;
				item["videoChannelTotal"] = info->m_channelInfo->m_videoChannelTotal;

				nlohmann::json channels = nlohmann::json::array();
				for (auto &ch : info->m_channelInfo->m_channels) {
					nlohmann::json c;
					c["logicChanId"] = ch.m_logicChanId;
					c["phyChanId"] = ch.m_phyChanId;
					c["type"] = ch.m_chanType;
					c["ptz"] = ch.m_ptz;
					channels.push_back(c);
				}
				item["channels"] = channels;
			}
			ts.push_back(item);
		}

		j["code"] = 0;
		j["msg"] = "success";
		j["result"] = ts;

		SendHttpRspEx(rtcMsg->sock.get(), j.dump());

	} else {
		MsHttpMsg rsp;
		rsp.m_version = httpMsg.m_version;
		rsp.m_status = "404";
		rsp.m_reason = "Not Found";
		SendHttpRspEx(rtcMsg->sock.get(), rsp);
	}
}
