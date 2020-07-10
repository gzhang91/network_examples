#ifndef _HTTP_CLIENT_H_
#define _HTTP_CLIENT_H_

#include <thread>
#include <mutex>
#include <list>
#include <queue>
#include <map>
#include <string>
#include <event2/bufferevent_ssl.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/http.h>

/*
本类主要用于libevent中HTTP客户端的封装
使用方法：
1. http_client运行之后会创建一个线程
2. 需要保证主线程不要退出，退出的时候一定要调用http_client.close()方法(建议按照http_client_test.cpp文件使用方法使用)

主要解决的问题：
1. 如果多个请求公用一个base，不能动态发送请求，只能先把请求准备好再发送(event_base_loop限制)
2. libevent使用异步模式，不能同步发送请求接收响应

类的功能：
1. loop线程用于处理IO，请求和响应都只能走loop线程，这方面就需要请求和响应都不能耗时而阻塞别的请求
2. 具有一个缓存队列，外界任务线程将任务投递进去，采用socketpair或者event_active来激活loop线程

后期新增功能：
1. 增加对每个request可以定制不同的cb
 */

namespace lib {

typedef void (*Callback)(int req_id, 
	const std::map<std::string, std::string> &headers,
	const std::string& body);

class HttpClient {
	struct Request {
		int req_id;
		std::string url;
		bool is_http;
		std::string host;
		int port;
		std::map<std::string, std::string> headers;
		std::string body;
	};

	typedef std::shared_ptr<Request> RequestPtr;

public:
	HttpClient();
	~HttpClient();

	bool start();
	void close();

	void push(RequestPtr req);
	void pop(RequestPtr &req);

	bool addRequst(
		int req_id,
		const std::string& url,
		const std::map<std::string, std::string> &headers, 
		const std::string& body);
	void setCallback(Callback cb) {
		main_cb_ = cb;
	}

private:
	bool sendRequest(RequestPtr request);
	bool loop();
	static void wakeUp(evutil_socket_t fd, short event, void *arg);
	static void httpRequestDone(struct evhttp_request *req, void *ctx);

private:
	// 与libevent关联的唯一的base_loop
	event_base *base_;

	// 停止执行
	bool stop_;

	// loop线程，用于处理io
	std::shared_ptr<std::thread> loop_thread_;
	// worker线程，用于接收请求，将请求转换成loop线程能够直接发送的
	// std::shared_ptr<std::thread> worker_thread_;

	// 任务队列加锁
	std::mutex lock_;
	// 任务队列
	std::queue<RequestPtr> request_queue_;
	// 需要注册的回调函数
	static Callback main_cb_;

	int pipe_fd_[2];

	// requst-->requst_id 用于区分从应用那边过来的request标记
	static std::map<struct evhttp_request*, int> request_info_;
};


}; //! namespace lib

#endif // !_HTTP_CLIENT_H_
