#ifndef MS_JT_SERVER_H
#define MS_JT_SERVER_H

#include "MsEvent.h"
#include "MsMsgDef.h"
#include "MsReactor.h"
#include "MsSocket.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace std;

// JT/T 808 Message IDs
enum JT808_MSG_ID {
	JT_TERMINAL_COMMON_RSP = 0x0001,   // Terminal common response
	JT_PLATFORM_COMMON_RSP = 0x8001,   // Platform common response
	JT_TERMINAL_HEARTBEAT = 0x0002,    // Terminal heartbeat
	JT_TERMINAL_REGISTER = 0x0100,     // Terminal registration
	JT_TERMINAL_REGISTER_RSP = 0x8100, // Terminal registration response
	JT_TERMINAL_UNREGISTER = 0x0003,   // Terminal logout
	JT_TERMINAL_AUTH = 0x0102,         // Terminal authentication
	JT_LOCATION_REPORT = 0x0200,       // Location report
	JT_AV_ATTR_QUERY = 0x9003,
	JT_AV_ATTR_QUERY_RSP = 0x1003,
	JT_LIVE_STREAM_REQ = 0x9101,
	JT_LIVE_STREAM_CONTROL = 0x9102,
	JT_PARAM_QUERY = 0x8106,
	JT_PARAM_QUERY_RSP = 0x0104,
};

// JT/T 808-2019 Message Header
struct JT808Header {
	uint16_t msgId;          // Message ID
	uint16_t msgBodyAttr;    // Message body attributes
	uint8_t protocolVersion; // Protocol version
	string terminalPhone;    // Terminal phone number (BCD, 11 bytes)
	uint16_t msgSerialNo;    // Message serial number

	uint16_t totalPackets; // Total packets (for split message)
	uint16_t packetNo;     // Packet number (for split message)

	bool hasVersionIdentifier() const { return (msgBodyAttr >> 14) & 0x01; }
	bool isSplit() const { return (msgBodyAttr >> 13) & 0x01; }
	bool isEncrypted() const { return (msgBodyAttr >> 10) & 0x07; }
	int bodyLength() const { return msgBodyAttr & 0x03FF; }
};

// JT/T 808 Location Data
struct JT808Location {
	uint32_t alarmFlag; // Alarm flags
	uint32_t status;    // Status
	uint32_t latitude;  // Latitude (x10^6)
	uint32_t longitude; // Longitude (x10^6)
	uint16_t altitude;  // Altitude (meters)
	uint16_t speed;     // Speed (x10 km/h)
	uint16_t direction; // Direction (0-359)
	string timestamp;   // BCD timestamp: YYMMDDhhmmss
};

// JT/T 808 Parsed Message
struct JT808Message {
	JT808Header header;
	vector<uint8_t> body;
	uint8_t checksum;

	// Parsed location data (if applicable)
	JT808Location location;
};

// JT/T 808 Server
class MsJtServer : public MsReactor {
public:
	MsJtServer(int type, int id);

	void Run() override;
	void HandleMsg(MsMsg &msg) override;

	// Process received JT808 message
	void HandleJtMessage(shared_ptr<MsEvent> evt, JT808Message &msg);

	// Send response to terminal
	void SendMessage(shared_ptr<MsSocket> sock, uint16_t msgId, const string &terminalPhone,
	                 const vector<uint8_t> &body);

	// Send platform common response
	void SendCommonResponse(shared_ptr<MsSocket> sock, const string &terminalPhone,
	                        uint16_t respSerialNo, uint16_t respMsgId, uint8_t result);

private:
	void HandleHttpMsg(shared_ptr<SHttpTransferMsg> rtcMsg);

	struct STerminalInfo {
		shared_ptr<MsSocket> m_sock;
		bool m_authenticated = false;
		string m_terminalId;
		string m_authCode;
		string m_plate;
		std::chrono::steady_clock::time_point m_lastHeartbeat;

		shared_ptr<SJtStreamInfo> m_avAttr;
		shared_ptr<SJtChannelInfo> m_channelInfo;

		MsMsg m_reqMsg;
		uint16_t m_waitSeq = UINT16_MAX;
		int m_waitTimerID = -1;
	};

	void RequestLiveStream(MsMsg &msg);
	void StopLiveStream(MsMsg &msg);

	void CheckStreamInfo(shared_ptr<STerminalInfo> &terminalInfo);
	void QueryAvChannel(shared_ptr<STerminalInfo> &terminalInfo);

	// Message handlers
	void OnTerminalRegister(shared_ptr<MsEvent> evt, JT808Message &msg,
	                        shared_ptr<STerminalInfo> &terminalInfo);
	void OnTerminalAuth(shared_ptr<MsEvent> evt, JT808Message &msg,
	                    shared_ptr<STerminalInfo> &terminalInfo);
	void OnTerminalHeartbeat(shared_ptr<MsEvent> evt, JT808Message &msg,
	                         shared_ptr<STerminalInfo> &terminalInfo);
	void OnLocationReport(shared_ptr<MsEvent> evt, JT808Message &msg,
	                      shared_ptr<STerminalInfo> &terminalInfo);
	void OnTerminalUnregister(shared_ptr<MsEvent> evt, JT808Message &msg,
	                          shared_ptr<STerminalInfo> &terminalInfo);
	void OnAvAttrQueryRsp(shared_ptr<MsEvent> evt, JT808Message &msg,
	                      shared_ptr<STerminalInfo> &terminalInfo);
	void OnParamQueryRsp(shared_ptr<MsEvent> evt, JT808Message &msg,
	                     shared_ptr<STerminalInfo> &terminalInfo);
	void OnTerminalCommonRsp(shared_ptr<MsEvent> evt, JT808Message &msg,
	                         shared_ptr<STerminalInfo> &terminalInfo);

	// Parse location from message body
	bool ParseLocation(const vector<uint8_t> &body, JT808Location &loc);

	// BCD encoding/decoding
	string BcdToString(const uint8_t *bcd, int len);
	vector<uint8_t> StringToBcd(const string &str, int len);

private:
	map<string, shared_ptr<STerminalInfo>> m_terminals; // terminalPhone -> terminal info
	uint16_t m_serialNo = 0;
};

#endif // MS_JT_SERVER_H
