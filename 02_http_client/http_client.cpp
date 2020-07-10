#include "http_client.h"
#include <unistd.h>
#include <string.h>
#include "event2/thread.h"

namespace lib {

Callback HttpClient::main_cb_ = nullptr;
std::map<struct evhttp_request*, int> HttpClient::request_info_;
std::map<struct evhttp_request*, struct evhttp_connection *> HttpClient::request_conn_;

HttpClient::HttpClient() {
	base_ = nullptr;
	stop_ = false;
	main_cb_ = nullptr;
}

HttpClient::~HttpClient() {
	if (base_) {
		event_base_free(base_);
	}
}

bool HttpClient::start() {
	evthread_use_pthreads();

	base_ = event_base_new();
	if (!base_) {
		perror("event_base_new()");
		return false;
	}

	pipe(pipe_fd_);
	
	loop_thread_.reset(new std::thread(&HttpClient::loop, this));
	loop_thread_->detach();
}

void HttpClient::close() {
	stop_ = true;
	event_base_loopbreak(base_);
}

void HttpClient::httpRequestDone(struct evhttp_request *req, void *ctx) {
	char buffer[256];
	int nread;

	if (!req || !evhttp_request_get_response_code(req)) {
		/* If req is NULL, it means an error occurred, but
		 * sadly we are mostly left guessing what the error
		 * might have been.  We'll do our best... */
		struct bufferevent *bev = (struct bufferevent *) ctx;
		unsigned long oslerr;
		int printed_err = 0;
		int errcode = EVUTIL_SOCKET_ERROR();
		fprintf(stderr, "some request failed - no idea which one though!\n");
		/* Print out the OpenSSL error queue that libevent
		 * squirreled away for us, if any. */
		while ((oslerr = bufferevent_get_openssl_error(bev))) {
			//ERR_error_string_n(oslerr, buffer, sizeof(buffer));
			fprintf(stderr, "%s\n", buffer);
			printed_err = 1;
		}
		/* If the OpenSSL error queue was empty, maybe it was a
		 * socket error; let's try printing that. */
		if (!printed_err)
			fprintf(stderr, "socket error = %s (%d)\n",
				evutil_socket_error_to_string(errcode),
				errcode);
		return;
	}

	fprintf(stderr, "Response line: %d %s\n",
	    evhttp_request_get_response_code(req),
	    evhttp_request_get_response_code_line(req));

	std::string buffer_info;
	while ((nread = evbuffer_remove(evhttp_request_get_input_buffer(req),
		    buffer, sizeof(buffer)))
	       > 0) {
		/* These are just arbitrary chunks of 256 bytes.
		 * They are not lines, so we can't treat them as such. */
		//fwrite(buffer, nread, 1, stdout);
		buffer_info.append(buffer);
	}

	std::map<struct evhttp_request*, int>::iterator it = request_info_.find(req);
	if (it == request_info_.end()) {
		// -1代表错误
		main_cb_(-1, std::map<std::string, std::string>(), "");
	} else {
		main_cb_(it->second, std::map<std::string, std::string>(), buffer_info);
	}

	std::map<struct evhttp_request*, struct evhttp_connection *>::iterator it_conn = request_conn_.find(req);
	if (it_conn != request_conn_.end()) {
		evhttp_connection_free(it_conn->second);
	}
}

bool HttpClient::addRequst(int req_id,
	const std::string& url_base,
	const std::map<std::string, std::string> &headers, 
	const std::string& body) {

	bool is_http = false;
	struct evhttp_uri *http_uri = NULL;
	const char *url = NULL;
	const char *scheme, *host, *path, *query;
	char uri[256];
	int port;
	
	http_uri = evhttp_uri_parse(url_base.c_str());
	if (http_uri == NULL) {
		printf("malformed url");
		return false;
	}

	scheme = evhttp_uri_get_scheme(http_uri);
	if (scheme == NULL || (strcasecmp(scheme, "https") != 0 &&
	                       strcasecmp(scheme, "http") != 0)) {
		printf("url must be http or https");
		return false;
	}

	host = evhttp_uri_get_host(http_uri);
	if (host == NULL) {
		printf("url must have a host");
		return false;
	}
	printf("host: %s\n", host);

	port = evhttp_uri_get_port(http_uri);
	if (port == -1) {
		port = (strcasecmp(scheme, "http") == 0) ? 80 : 443;
	}
	printf("port: %d\n", port);

	path = evhttp_uri_get_path(http_uri);
	if (strlen(path) == 0) {
		path = "/";
	}
	printf("path: %s\n", path);

	query = evhttp_uri_get_query(http_uri);
	if (query == NULL) {
		snprintf(uri, sizeof(uri) - 1, "%s", path);
	} else {
		snprintf(uri, sizeof(uri) - 1, "%s?%s", path, query);
	}
	uri[sizeof(uri) - 1] = '\0';
	printf("uri: %s\n", uri);
	
	if (strcasecmp(scheme, "http") == 0) {
		is_http = true;
	} else {
		is_http = false;
	}

	RequestPtr req(new Request);
	req->req_id = req_id;
	req->url = uri;
	req->host = host;
	req->port = port;
	req->is_http = is_http;
	req->headers = std::move(headers);
	req->body = std::move(body);
	push(req);	

	write(pipe_fd_[1], "1", 1);

	if (http_uri) {
		evhttp_uri_free(http_uri);
	}

	return true;
}

bool HttpClient::sendRequest(RequestPtr request) {
	Request *req = request.get();
	if (!req) {
		return false;
	} 

	struct bufferevent *bev = nullptr;

	if (req->is_http) {
		bev = bufferevent_socket_new(base_, -1, BEV_OPT_CLOSE_ON_FREE);
	} else {
		/*bev = bufferevent_openssl_socket_new(base_, -1, ssl,
			BUFFEREVENT_SSL_CONNECTING,
			BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);
		*/
	}

	if (bev == NULL) {
		fprintf(stderr, "bufferevent_openssl_socket_new() failed\n");
		return false;
	}

	bufferevent_openssl_set_allow_dirty_shutdown(bev, 1);

	// For simplicity, we let DNS resolution block. Everything else should be
	// asynchronous though.
	struct evhttp_connection *evcon = evhttp_connection_base_bufferevent_new(base_, NULL, bev,
		req->host.c_str(), req->port);
	if (evcon == NULL) {
		fprintf(stderr, "evhttp_connection_base_bufferevent_new() failed\n");
		return false;
	}

	evhttp_connection_set_timeout(evcon, 10);

	// Fire off the request
	struct evhttp_request *http_req = evhttp_request_new(httpRequestDone, bev);
	if (http_req == NULL) {
		fprintf(stderr, "evhttp_request_new() failed\n");
		evhttp_connection_free(evcon);
		return false;
	}

	struct evkeyvalq *output_headers = evhttp_request_get_output_headers(http_req);
	evhttp_add_header(output_headers, "Host", req->host.c_str());
	evhttp_add_header(output_headers, "Connection", "close");

	std::map<std::string, std::string>::iterator it = req->headers.begin();
	for (; it != req->headers.end(); it++) {
		evhttp_add_header(output_headers, it->first.c_str(), it->second.c_str());
	}

	request_info_.insert(std::make_pair(http_req, req->req_id));
	request_conn_.insert(std::make_pair(http_req, evcon));

	int r = evhttp_make_request(evcon, http_req, 
		req->body.empty() == false ? EVHTTP_REQ_POST : EVHTTP_REQ_GET, req->url.c_str());
	if (r != 0) {
		fprintf(stderr, "evhttp_make_request() failed\n");
		evhttp_connection_free(evcon);
		return false;
	}	

	return true;
}

void HttpClient::push(RequestPtr req) {
	std::lock_guard<std::mutex> guard(lock_);
	request_queue_.push(req);
}

void HttpClient::pop(RequestPtr& ptr) {
	std::lock_guard<std::mutex> guard(lock_);
	ptr = request_queue_.front();
	request_queue_.pop();
	//return req;
}

void HttpClient::wakeUp(evutil_socket_t fd, short event, void *arg) {
	char data; 
	read(fd, &data, sizeof(char));

	HttpClient *clt = (HttpClient *)arg;
	// fixed: 这里需要没插入一个请求，就wakeUp一次，可以优化
	RequestPtr req ;
	clt->pop(req);
	clt->sendRequest(req);
}

bool HttpClient::loop() {
	struct event* ev_read = event_new(base_, pipe_fd_[0], EV_READ | EV_PERSIST, wakeUp, this);
	event_add(ev_read, NULL);

	event_base_loop(base_, EVLOOP_NO_EXIT_ON_EMPTY);
}

	
}; //! namespace lib
