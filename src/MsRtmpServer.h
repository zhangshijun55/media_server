#ifndef MS_RTMP_SERVER_H
#define MS_RTMP_SERVER_H

#include "MsMediaSource.h"
#include "MsMsg.h"
#include "MsReactor.h"
#include "MsRtmpMsg.h"

class MsRtmpServer : public MsReactor {
public:
	MsRtmpServer(int type, int id) : MsReactor(type, id) {}
	~MsRtmpServer() = default;

	void HandleMsg(MsMsg &msg) override;

	void RegistSource(const string &streamID, shared_ptr<MsMediaSource> source);
	void UnregistSource(const string &streamID);
	shared_ptr<MsMediaSource> GetRtmpSource(const string &streamID);

private:
	map<string, shared_ptr<MsMediaSource>> m_rtmpSources;
};

#endif // MS_RTMP_SERVER_H