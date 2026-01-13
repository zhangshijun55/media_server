#ifndef MS_HTTP_STREAM_H
#define MS_HTTP_STREAM_H
#include "MsMsgDef.h"
#include "MsReactor.h"
#include <string>
#include <vector>

class MsHttpStream : public MsReactor {
public:
	using MsReactor::MsReactor;
	void HandleMsg(MsMsg &msg) override;

private:
	void HandleStreamMsg(SHttpTransferMsg *httpMsg);

private:
	int m_seqID = 0;
};

#endif // MS_HTTP_STREAM_H
