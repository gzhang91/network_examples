#include "timeout_object.h"

TimeoutObject::TimeoutObject(Callback cb, int timespan, void *obj) :
	operate_func_(cb),
	obj_(obj) {
	if (timespan > 0) {
		timeout_expiration_ = time(NULL) + timespan;
	} else {
		timeout_expiration_ = time(NULL) - timespan;
	}
}

void TimeoutObject::TimeoutOperate() {
	operate_func_();
}

time_t TimeoutObject::GetTimeoutExpiration() {
	return timeout_expiration_;
}
