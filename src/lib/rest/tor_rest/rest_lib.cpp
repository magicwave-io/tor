#include "rest_lib.h"
#define JSMN_HEADER
#include "tor_rest_service.h"
#include <sstream>

static tor_rest_service *s_service_ptr = NULL;
static rest_server *s_server_ptr = NULL;

extern "C" int runServer(
		int port,
		void (*log_function)(const char *message),
		int (*handler)(tor_http_api_request_t *request) /*= nullptr*/
	)
{	  
	int threads = 2;
	int connection_limit = 100;

	s_service_ptr = new tor_rest_service(
		log_function,
		handler);

	s_server_ptr = new rest_server();
	
	// server->set_log_file(&cerr);
	s_server_ptr->set_max_connections(connection_limit);
	s_server_ptr->set_threads(threads);
  
	if (!s_server_ptr->start(s_service_ptr, port)) {
		std::stringstream msg;
		msg << "Cannot start REST server on port: " << port;
		if (nullptr != log_function)
			log_function(msg.str().c_str());
		else
			cerr << msg.str() << std::endl;
		return -1;
	}
	return 0;
}

extern "C" int stopServer(void)
{
	try {
		if(s_server_ptr) {
			s_server_ptr->stop();
			delete s_server_ptr;
			s_server_ptr = NULL;
		}
		if(s_service_ptr) {
			delete s_service_ptr;
			s_service_ptr = NULL;
		}
	}
	catch (...) {
		return -1;
	}
	return 0;
}