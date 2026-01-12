#pragma once
#include "MsMediaSource.h"
#include "MsMsgDef.h"

class MsRtspSource : public MsMediaSource, public std::enable_shared_from_this<MsRtspSource> {
public:
	MsRtspSource(const std::string &streamID, const std::string &url)
	    : MsMediaSource(streamID), m_url(url) {}

	void Work() override;
	shared_ptr<MsMediaSource> GetSharedPtr() override {
		return dynamic_pointer_cast<MsMediaSource>(shared_from_this());
	}

private:
	void OnRun();

private:
	std::string m_url;
};
