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
	void (*m_routeCreator)     (const char* targetNode, tor_route *route);
	int       (*m_commandProcessor) (tor_command* command);
	int       (*m_commandProcessorReplay) (tor_command_replay * command);
	int       (*commandProcessingCompleted) (payment_completed * command);

	std::string* route2json(tor_route *route);
	jsmn_parser m_jsonParser;
	
 public:
	tor_rest_service(
        void (*routeFunction)(const char* targetNode, tor_route *route),
        int (*commandProcessingFunction)(tor_command* command),
        int (*commandProcessingReplayFunction)(tor_command_replay * command),
        int (*commandProcessingCompletedFunction)(payment_completed *command));

	virtual bool handle(ufal::microrestd::rest_request& req) override;
	
};



