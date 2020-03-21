#ifndef __TIMEOUT_OBJECT_H__
#define __TIMEOUT_OBJECT_H__

#include <functional>

typedef std::function<void()> Callback;

class TimeoutObject {
public:
	TimeoutObject(Callback cb, int timespan, void *obj);

	void TimeoutOperate();
	time_t GetTimeoutExpiration();

private:
	Callback operate_func_;
	time_t timeout_expiration_;
	void *obj_;
};

#endif // __TIMEOUT_OBJECT_H__