#ifndef MS_FILE_SOURCE_H
#define MS_FILE_SOURCE_H
#include "MsMediaSource.h"
#include "MsMsgDef.h"

class MsFileSource : public MsMediaSource, public std::enable_shared_from_this<MsFileSource> {
public:
	MsFileSource(const std::string &streamID, const std::string &filename)
	    : MsMediaSource(streamID), m_filename(filename) {}

	void Work() override;
	shared_ptr<MsMediaSource> GetSharedPtr() override {
		return dynamic_pointer_cast<MsMediaSource>(shared_from_this());
	}

private:
	void OnRun();

private:
	int64_t m_lastPts = INT64_MAX;
	int64_t m_lastMs = 0;
	std::string m_filename;
};

#endif // MS_FILE_SOURCE_H
