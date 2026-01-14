/**
 * Copyright (c) 2020 Paul-Louis Ageneau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "MsThreadPool.h"
#include <optional>

// scope_guard helper
class scope_guard final {
public:
	scope_guard(std::function<void()> func) : function(std::move(func)) {}
	scope_guard(scope_guard &&other) = delete;
	scope_guard(const scope_guard &) = delete;
	void operator=(const scope_guard &) = delete;

	~scope_guard() {
		if (function)
			function();
	}

private:
	std::function<void()> function;
};

MsThreadPool &MsThreadPool::Instance() {
	static MsThreadPool *instance = new MsThreadPool;
	return *instance;
}

MsThreadPool::MsThreadPool() {}

MsThreadPool::~MsThreadPool() {}

int MsThreadPool::count() const {
	std::unique_lock lock(mWorkersMutex);
	return int(mWorkers.size());
}

void MsThreadPool::spawn(int count) {
	std::unique_lock lock(mWorkersMutex);
	while (count-- > 0)
		mWorkers.emplace_back(std::bind(&MsThreadPool::run, this));
}

void MsThreadPool::join() {
	{
		std::unique_lock lock(mMutex);
		mWaitingCondition.wait(lock, [&]() { return mBusyWorkers == 0; });
		mJoining = true;
		mTasksCondition.notify_all();
	}

	std::unique_lock lock(mWorkersMutex);
	for (auto &w : mWorkers)
		w.join();

	mWorkers.clear();

	mJoining = false;
}

void MsThreadPool::clear() {
	std::unique_lock lock(mMutex);
	while (!mTasks.empty())
		mTasks.pop();
}

void MsThreadPool::run() {
	++mBusyWorkers;
	scope_guard guard([&]() { --mBusyWorkers; });
	while (runOne()) {
	}
}

bool MsThreadPool::runOne() {
	if (auto task = dequeue()) {
		task();
		return true;
	}
	return false;
}

std::function<void()> MsThreadPool::dequeue() {
	std::unique_lock lock(mMutex);
	while (!mJoining) {
		std::optional<clock::time_point> time;
		if (!mTasks.empty()) {
			time = mTasks.top().time;
			if (*time <= clock::now()) {
				auto func = std::move(mTasks.top().func);
				mTasks.pop();
				return func;
			}
		}

		--mBusyWorkers;
		scope_guard guard([&]() { ++mBusyWorkers; });
		mWaitingCondition.notify_all();
		if (time)
			mTasksCondition.wait_until(lock, *time);
		else
			mTasksCondition.wait(lock);
	}
	return nullptr;
}
