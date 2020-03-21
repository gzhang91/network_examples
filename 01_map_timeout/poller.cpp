#include "poller.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

Poller::Poller() :
	poll_fd_(epoll_create1(EPOLL_CLOEXEC)),
	events_(16) {
	if (poll_fd_ < 0) {
		printf("error\n");
		abort();
	}
}

Poller::~Poller() {
	close(poll_fd_);
}

void Poller::AddEvent(int fd, int events, void *obj) {
	struct epoll_event event;
	memset(&event, 0, sizeof event);
	event.events = events;
	event.data.ptr = obj;
	
	if (epoll_ctl(poll_fd_, EPOLL_CTL_ADD, fd, &event) < 0)
	{
		printf("epoll_ctl add failed\n");
	}
}

void Poller::DelEvent(int fd) {
	struct epoll_event event;
	memset(&event, 0, sizeof event);

	if (epoll_ctl(poll_fd_, EPOLL_CTL_DEL, fd, &event) < 0)
	{
		printf("epoll_ctl del failed\n");
	}
}

void Poller::ModEvent(int fd, int events, void *obj) {
	struct epoll_event event;
	memset(&event, 0, sizeof event);
	event.events = events;
	event.data.ptr = obj;

	if (epoll_ctl(poll_fd_, EPOLL_CTL_MOD, fd, &event) < 0)
	{
		printf("epoll_ctl modify failed\n");
	}
}

bool Poller::WaitEvent(std::vector<void *> &res_obj) {
	do {

		int num_events = epoll_wait(poll_fd_, &events_[0], (int)events_.size(), 1000);

		if (num_events > 0) {
			for (int i = 0; i < num_events; i++) {
				res_obj.push_back(events_[i].data.ptr);
			}

			if (num_events == (int)events_.size()) {
				events_.resize(events_.size() * 2);
			}
		} else if (num_events == 0) {
			printf("timeout ...\n");
		} else {
			if (errno != EINTR) {
				printf("error happend[%d]\n", errno);
				return false;
			}
		}

	} while (0);

	return true;
}
