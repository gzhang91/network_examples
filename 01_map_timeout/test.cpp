#include <iostream>
#include <sys/epoll.h>
#include "poller.h"
#include "timeout_manager.h"
#include "timeout_object.h"

using namespace std;

void Print() {
	cout << "hahah " << endl;
}

int main() {

	Poller poller;
	TimeoutManager timeout_manager;
	TimeoutObject *obj1 = timeout_manager.AddTimeoutObj(Print, 2, NULL);
	(void)obj1;

	TimeoutObject *obj2 = timeout_manager.AddTimeoutObj(Print, 4, NULL);
	(void)obj2;

	TimeoutObject *obj3 = timeout_manager.AddTimeoutObj(Print, 5, NULL);
	(void)obj3;

	TimeoutObject *obj4 = timeout_manager.AddTimeoutObj(Print, 6, NULL);
	(void)obj4;

	TimeoutObject *obj5 = timeout_manager.AddTimeoutObj(Print, 1, NULL);
	(void)obj5;

	poller.AddEvent(timeout_manager.GetTimerFd(), EPOLLIN, &timeout_manager);

	TimeoutObject *obj6 = timeout_manager.AddTimeoutObj(Print, 4, NULL);
	(void)obj6;

	timeout_manager.DelTimeoutObj(obj4);

	std::vector<void *> res;
	while (1) {
		res.clear();
		bool ret = poller.WaitEvent(res);
		if (!ret) {
			printf("error waitEvent\n");
			break;
		}

		if (!res.empty()) {
			for (int i = 0; i < res.size(); i++) {
				TimeoutManager *obj = (TimeoutManager *)res[i];
				obj->HandleRead();
			}
		}
	}

	return 0;
}