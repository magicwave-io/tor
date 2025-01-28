#include "rest_lib.h"
#define JSMN_HEADER
#include "jsmn.h"
#include "tor_rest_service.h"


extern "C" int runServer(int port, void (*routeFunction)(const char* targetNode, tor_route *route),
        int (*commandProcessingFunction)(tor_command* command),
                         int (*commandProcessingReplayFunction)(tor_command_replay* command),
                         int (*commandProcessingCompletedFunction)(payment_completed *command),
						 const char *appVersionString /*= NULL*/)
{	  
	int threads = 2;
	int connection_limit = 100;

	auto service = new tor_rest_service(
		routeFunction,
		commandProcessingFunction,
		commandProcessingReplayFunction,
		commandProcessingCompletedFunction,
		static_cast<const char *>(appVersionString));

	//auto rest_server server;
	auto server = new rest_server();
	
	server->set_log_file(&cerr);
	server->set_max_connections(connection_limit);
	server->set_threads(threads);
  
	if (!server->start(service, port))
		return cerr << "Cannot start REST server!" << endl, 1;
	
	//server.wait_until_signalled();
	return 0;
}
