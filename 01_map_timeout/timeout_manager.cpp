#include "timeout_manager.h"
#include <sys/timerfd.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <vector>
#include <time.h>
#include <string.h>
#include "timeout_object.h"

TimeoutManager::TimeoutManager() {
	timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	if (timer_fd_ < 0) {
		abort();
	}
}

TimeoutManager::~TimeoutManager() {
	close(timer_fd_);

}

void TimeoutManager::HandleRead() {
	uint64_t time_read;
  	ssize_t n = read(timer_fd_, &time_read, sizeof time_read);
  	// printf("n=%lld, time_read=%lld, errno=%d\n", n, time_read, errno);

  	std::vector<TimeEntryNode> expired;

  	// 得到超时的序列
  	time_t now = time(0);
	TimeEntryNode sentry(now, reinterpret_cast<TimeoutObject*>(UINTPTR_MAX)); 
	TimeEntry::iterator end = time_entry_.lower_bound(sentry);
	std::copy(time_entry_.begin(), end, back_inserter(expired));
	time_entry_.erase(time_entry_.begin(), end);

	// 从TimeoutEntry列中删除到期的节点
	// printf("expired: %zu\n", expired.size());
	for (const TimeEntryNode& it : expired)
	{
		// printf("item: %zu, %p\n", it.second->GetTimeoutExpiration(), it.second);
		TimeoutEntryNode node(it.second, it.second->GetTimeoutExpiration());
		size_t n = timeout_entry_.erase(node);
		assert(n == 1); (void)n;
	}

	// 回调到期节点的函数
	for (const TimeEntryNode& it : expired)
	{
		it.second->TimeoutOperate();
		delete it.second;
	}

	// 重新开始下一个周期节点的操作
	if (!time_entry_.empty())
	{
		time_t next_expire = time_entry_.begin()->second->GetTimeoutExpiration() - time(0);

		struct itimerspec new_value;
		struct itimerspec old_value;
		memset(&new_value, 0, sizeof new_value);
		memset(&old_value, 0, sizeof old_value);
		new_value.it_value.tv_sec = next_expire;
		int ret = ::timerfd_settime(timer_fd_, 0, &new_value, &old_value);
		if (ret != 0) {
			printf("ret= %d\n", ret);
		}
	}
}

// 本案例并不提供毫秒,微妙级别的定时操作
TimeoutObject* TimeoutManager::AddTimeoutObj(Callback cb, int timespan, void *obj_param) {
	TimeoutObject *obj = new TimeoutObject(std::move(cb), timespan, obj_param);
	if (!obj) {
		// add log
		return NULL;
	}

	time_t timeout_stamp = obj->GetTimeoutExpiration();
	//printf("timestamp: %lld\n", timeout_stamp);

	bool need_update_time = false;
	do {
		// 判断是否为空或者是否比第一个节点超时时间都接近了
		TimeEntry::iterator it_time = time_entry_.begin();
		if (it_time == time_entry_.end() || timeout_stamp < it_time->first) {
			printf("update time ... \n");
			need_update_time = true;
		}

		//printf("insert: %lld, %p\n", timeout_stamp, obj);
		time_entry_.insert(TimeEntryNode(timeout_stamp, obj));
		timeout_entry_.insert(TimeoutEntryNode(obj, timeout_stamp));

		if (time_entry_.size() != timeout_entry_.size()) {
			printf("not same\n");
			delete obj;
			abort();
		}

	} while (0);

	if (need_update_time) {
		struct itimerspec new_value;
		struct itimerspec old_value;
		memset(&new_value, 0, sizeof new_value);
		memset(&old_value, 0, sizeof old_value);
		new_value.it_value.tv_sec = timespan;
		new_value.it_value.tv_nsec = 0;
		int ret = timerfd_settime(timer_fd_, 0, &new_value, &old_value);
		if (ret != 0) {
			printf("ret= %d\n", ret);
			delete obj;
			return NULL;
		}
	}

	return obj;
}

void TimeoutManager::DelTimeoutObj(TimeoutObject *obj) {
	TimeoutEntryNode timeout_entry_node(obj, obj->GetTimeoutExpiration());
	TimeoutEntry::iterator it = timeout_entry_.find(timeout_entry_node);
	if (it != timeout_entry_.end())
	{
		size_t n = time_entry_.erase(TimeEntryNode(it->first->GetTimeoutExpiration(), it->first));
		assert(n == 1); (void)n;
		delete it->first;
		timeout_entry_.erase(it);
	}
}

int TimeoutManager::ObjSize() {
	return time_entry_.size();
}