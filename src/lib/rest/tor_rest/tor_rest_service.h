#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include "microrestd.h"

using namespace std;
using namespace ufal::microrestd;

class tor_rest_service : public rest_service
{
private:	
	void (*m_log_handler)(const char *message);
	int (*m_handler)(tor_http_api_request_t *);
	void log(const char *message);
	void req_log(rest_request& req);

 public:
	tor_rest_service(
		void (*log_function)(const char *message),
		int (*handler)(tor_http_api_request_t *request) = nullptr
	);

	virtual bool handle(ufal::microrestd::rest_request& req) override;
	
};



