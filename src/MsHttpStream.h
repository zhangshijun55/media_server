#pragma once
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
