#ifndef MS_RES_MANAGER_H
#define MS_RES_MANAGER_H

#include "MsMediaSource.h"
#include <map>

class MsResManager {
public:
	static MsResManager &GetInstance();

	void AddMediaSource(const std::string &key, std::shared_ptr<MsMediaSource> source);
	void RemoveMediaSource(const std::string &key);
	std::shared_ptr<MsMediaSource> GetMediaSource(const std::string &key);

	shared_ptr<MsMediaSource> GetOrCreateMediaSource(const std::string &type,
	                                                 const std::string &key,
	                                                 const std::string &streamInfo,
	                                                 shared_ptr<MsMediaSink> sink);

private:
	MsResManager() = default;
	~MsResManager() = default;

	MsResManager(const MsResManager &) = delete;
	MsResManager &operator=(const MsResManager &) = delete;

	static std::mutex m_instanceMutex;
	static MsResManager *m_instance;

	std::mutex m_mapMutex;
	std::map<std::string, std::shared_ptr<MsMediaSource>> m_mediaSources;
};

#endif // MS_RES_MANAGER_H
