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
	void (*m_routeCreator) (const char* targetNode, tor_route *route);
	int  (*m_commandProcessor) (tor_command* command);
	int  (*m_commandProcessorReplay) (tor_command_replay * command);
	int  (*m_commandProcessingCompleted) (payment_completed * command);
	std::string app_version = "undefined";
	void (*m_log_handler)(const char *message);
	void log(const char *message);
	void req_log(rest_request& req);

	std::string* route2json(tor_route *route);
 public:
	tor_rest_service(
        void (*routeFunction)(const char* targetNode, tor_route *route),
        int (*commandProcessingFunction)(tor_command* command),
        int (*commandProcessingReplayFunction)(tor_command_replay * command),
        int (*commandProcessingCompletedFunction)(payment_completed *command),
		void (*log_function)(const char *message),
		const char *app_version_string = NULL
	);

	virtual bool handle(ufal::microrestd::rest_request& req) override;
	
};



