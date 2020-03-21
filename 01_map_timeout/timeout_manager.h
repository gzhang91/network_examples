#ifndef __TIMEOUT_MANAGER_H__
#define __TIMEOUT_MANAGER_H__

#include <set>
#include <time.h>
#include <functional>

typedef std::function<void()> Callback;

class TimeoutObject;
class TimeoutManager {
public:
	TimeoutManager();
	~TimeoutManager();

	TimeoutObject* AddTimeoutObj(Callback cb, int timespan, void *obj);
	void DelTimeoutObj(TimeoutObject *obj);
	int ObjSize();
	int GetTimerFd() {
		return timer_fd_;
	}

	void HandleRead();	

private:
	// set是默认排序的,TimeEntry默认第一个是最先超时的
	typedef std::pair<time_t, TimeoutObject*> TimeEntryNode;
	typedef std::set<TimeEntryNode> TimeEntry;
	typedef std::pair<TimeoutObject*, time_t> TimeoutEntryNode;
	typedef std::set<TimeoutEntryNode> TimeoutEntry;

	TimeEntry time_entry_;
	TimeoutEntry timeout_entry_;
	int timer_fd_;
};

# endif // __TIMEOUT_MANAGER_H__