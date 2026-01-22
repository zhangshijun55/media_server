#ifndef MS_JT_HANDLER_H
#define MS_JT_HANDLER_H

#include "MsEvent.h"
#include "MsJtServer.h"
#include <memory>
#include <vector>

using namespace std;

// JT/T 808 Connection Handler
class MsJtHandler : public MsEventHandler {
public:
	MsJtHandler(const shared_ptr<MsJtServer> &server);
	~MsJtHandler();

	void HandleRead(shared_ptr<MsEvent> evt) override;
	void HandleClose(shared_ptr<MsEvent> evt) override;
	void ProcessBuffer(shared_ptr<MsEvent> evt);

private:
	// Unescape JT808 data (0x7d 0x02 -> 0x7e, 0x7d 0x01 -> 0x7d)
	void Unescape(const uint8_t *data, int len, vector<uint8_t> &result);

	// Escape JT808 data for sending
	vector<uint8_t> Escape(const uint8_t *data, int len);

	// Calculate XOR checksum
	uint8_t CalcChecksum(const uint8_t *data, int len);

	// Parse message header
	bool ParseHeader(const vector<uint8_t> &data, JT808Header &header, int &headerLen);

	// Parse complete message
	bool ParseMessage(const vector<uint8_t> &data, JT808Message &msg);

public:
	unique_ptr<uint8_t[]> m_bufPtr;
	int m_bufSize;
	int m_bufOff;

private:
	shared_ptr<MsJtServer> m_server;
	string m_terminalId; // Authenticated terminal ID
};

#endif // MS_JT_HANDLER_H
