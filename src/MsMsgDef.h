#pragma once
#include "MsHttpMsg.h"
#include <stdint.h>
#include <string>

using namespace std;

enum MS_MSG_ID {
	MS_REG_TIME_OUT = 0x100,
	MS_INIT_CATALOG,
	MS_HTTP_INIT_CATALOG,
	MS_CATALOG_TIME_OUT,
	MS_INIT_RECORD,
	MS_RECORD_TIME_OUT,
	MS_GEN_HTTP_RSP,
	MS_INIT_INVITE,
	MS_INVITE_CALL_RSP,
	MS_INVITE_TIME_OUT,
	MS_MEDIA_NODE_TIMER,
	MS_PTZ_CONTROL,
	MS_ONVIF_PROBE,
	MS_ONVIF_PROBE_TIMEOUT,
	MS_ONVIF_PROBE_FINISH,
	MS_QUERY_PRESET,
	MS_QUERY_PRESET_TIMEOUT,
	MS_GB_SERVER_HANDLER_CLOSE,
	MS_GET_REGIST_DOMAIN,
	MS_STOP_INVITE_CALL,
	MS_RTC_MSG,
	MS_RTC_PEER_CLOSED,
};

enum MS_SERVICE_TYPE {
	MS_HTTP_SERVER = 1,
	MS_HTTP_CLIENT,
	MS_GB_SERVER,
	MS_GB_SOURCE,
	MS_FILE_SOURCE,
	MS_HTTP_STREAM,
	MS_RTSP_SOURCE,
	MS_RTSP_SERVER,
	MS_RTC_SERVER,
};

enum TRASNSPORT { EN_UDP = 0, EN_TCP_ACTIVE, EN_TCP_PASSIVE };

struct SGbContext {
	string gbID;
	string gbCallID;
	string rtpIP;
	int rtpPort;
	int transport;
	int type;
	string startTime;
	string endTime;
};

struct SData {
	uint8_t *m_buf;
	int m_len;
};

struct SMediaNode {
	int node_id;
	int idle;
	int m_lastUsed;
	int httpStreamPort;
	int rtspPort;
	int httpPort;
	string httpMediaIP;
	string nodeIp;
};

class SPtzCmd {
public:
	string m_devid;
	string m_presetID;
	int m_ptzCmd;
	int m_timeout;
};

struct SRtcMsg {
	MsHttpMsg httpMsg;
	string sdp;
	shared_ptr<MsSocket> sock;
};
