#include <unistd.h>
#include <iostream>
#include "http_client.h"

using namespace std;

void httpCallback(int req_id, 
	const std::map<std::string, std::string> &headers,
	const std::string& body) {
	if (req_id == -1) {
		printf("error accured: %d\n", req_id);
		return;
	}

	printf("req_id: %d\n", req_id);
	printf("body: %s\n", body.c_str());
}

int main() {
	lib::HttpClient *clt = new lib::HttpClient;
	clt->setCallback(httpCallback);
	clt->start();

	int cnt = 1;
	while (1) {
		clt->addRequst(cnt++, "http://127.0.0.1/test", std::map<std::string, std::string>(), "");
		clt->addRequst(cnt++, "http://127.0.0.1/", std::map<std::string, std::string>(), "");
		clt->addRequst(cnt++, "http://127.0.0.1/", std::map<std::string, std::string>(), "");
		clt->addRequst(cnt++, "http://127.0.0.1/", std::map<std::string, std::string>(), "");
		sleep(1);

		if (cnt == 20) {
			break;
		}
	}

	clt->close();

	return 0;
}
