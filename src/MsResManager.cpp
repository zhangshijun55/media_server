#include "MsResManager.h"
#include "MsSourceFactory.h"

// Static member initialization
std::mutex MsResManager::m_instanceMutex;
MsResManager *MsResManager::m_instance = nullptr;

MsResManager &MsResManager::GetInstance() {
	if (m_instance == nullptr) {
		std::lock_guard<std::mutex> lock(m_instanceMutex);
		if (m_instance == nullptr) {
			m_instance = new MsResManager();
		}
	}
	return *m_instance;
}

void MsResManager::AddMediaSource(const std::string &key, std::shared_ptr<MsMediaSource> source) {
	if (key.empty() || !source) {
		return;
	}

	std::lock_guard<std::mutex> lock(m_mapMutex);
	m_mediaSources[key] = source;
}

void MsResManager::RemoveMediaSource(const std::string &key) {
	if (key.empty()) {
		return;
	}

	std::lock_guard<std::mutex> lock(m_mapMutex);
	m_mediaSources.erase(key);
}

std::shared_ptr<MsMediaSource> MsResManager::GetMediaSource(const std::string &key) {
	if (key.empty()) {
		return nullptr;
	}

	std::lock_guard<std::mutex> lock(m_mapMutex);
	auto it = m_mediaSources.find(key);
	if (it != m_mediaSources.end()) {
		return it->second;
	}
	return nullptr;
}

shared_ptr<MsMediaSource> MsResManager::GetOrCreateMediaSource(const std::string &type,
                                                               const std::string &streamID,
                                                               const std::string &streamInfo,
                                                               shared_ptr<MsMediaSink> sink) {
	std::shared_ptr<MsMediaSource> source;
	if (type == "live") {
		source = this->GetMediaSource(streamID);
		if (source) {
			source->AddSink(sink);
			return source;
		} else {
			source = MsSourceFactory::CreateLiveSource(streamID);
			if (!source) {
				MS_LOG_WARN("create source failed for stream: %s", streamID.c_str());
				return nullptr;
			} else {
				source->AddSink(sink);
				source->Work();
				return source;
			}
		}
	} else if (type == "vod") {
		source = this->GetMediaSource(streamID);
		if (source) {
			MS_LOG_WARN("vod source already exists for stream: %s", streamID.c_str());
			return nullptr;
		}

		source = MsSourceFactory::CreateVodSource(streamID, streamInfo);
		if (!source) {
			MS_LOG_WARN("create source failed for stream: %s", streamID.c_str());
			return nullptr;
		} else {
			source->AddSink(sink);
			source->Work();
			return source;
		}
	} else if (type == "gbvod") {
		source = this->GetMediaSource(streamID);
		if (source) {
			MS_LOG_WARN("gbvod source already exists for stream: %s", streamID.c_str());
			return nullptr;
		}

		source = MsSourceFactory::CreateGbvodSource(streamID, streamInfo);
		if (!source) {
			MS_LOG_WARN("create source failed for stream: %s", streamID.c_str());
			return nullptr;
		} else {
			source->AddSink(sink);
			source->Work();
			return source;
		}
	} else {
		MS_LOG_WARN("unsupported source type: %s", type.c_str());
		return nullptr;
	}
}
