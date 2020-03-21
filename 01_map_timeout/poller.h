#ifndef __POLLER_H__
#define __POLLER_H__

#include <sys/epoll.h>
#include <vector>



class Poller {
	typedef std::vector<struct epoll_event> EventSets;

public:
	Poller();
	~Poller();

	void AddEvent(int fd, int events, void *obj);
	void DelEvent(int fd);
	void ModEvent(int fd, int events, void *obj);
	bool WaitEvent(std::vector<void *> &res_obj);

private:
	int poll_fd_;
	EventSets events_;
};

#endif // __POLLER_H__