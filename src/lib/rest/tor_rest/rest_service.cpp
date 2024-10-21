#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include <string.h>
#include <stdio.h>

#include "microrestd.h"
#include "rest_lib.h"
#include "jsmn.h"
#include "tor_rest_service.h"
#include "route_generator.h"


using namespace std;
using namespace ufal::microrestd;


static int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}

char * my_strndup (const char *s, size_t n)
{
	size_t len = strnlen (s, n);
	char *newString = (char *) malloc (len + 1);

	if (n == NULL)
		return NULL;

	newString[len] = '\0';
	return (char *) memcpy (newString, s, len);
}

std::string* tor_rest_service::route2json(tor_route *route)
{
	json_builder json;

	json.object();
	if (nullptr != route->nodes && route->nodes_len >= 3) {
		json.key("node1").value(route->nodes[0].node_id);
		json.key("node2").value(route->nodes[1].node_id);
		json.key("node3").value(route->nodes[1].node_id);
	}
    json.finish();


	const auto str = json.current();
		
	return new string(str.str,str.len);
}

tor_rest_service::tor_rest_service(void (* routeFunction)(const char* targetNode, tor_route *route),
        int (* commandProcessingFunction)(tor_command* command),
                                   int (* commandProcessingReplayFunction)(tor_command_replay* command),
                                   int (*commandProcessingCompletedFunction)(payment_completed *command))
{
	m_routeCreator = routeFunction;
	m_commandProcessor = commandProcessingFunction;
	m_commandProcessorReplay = commandProcessingReplayFunction;
    commandProcessingCompleted = commandProcessingCompletedFunction;

	jsmn_init(&m_jsonParser);
}

bool tor_rest_service::handle(rest_request& req) 
{
	const int ConstMaxJsonTokens = 1280;
	jsmntok_t t[ConstMaxJsonTokens];
    jsmn_init(&m_jsonParser);
    if (req.method != "HEAD" && req.method != "GET" && req.method != "POST") 
        return req.respond_method_not_allowed("HEAD, GET, POST");

	auto strRoutePrefix = string("/api/paymentRoute");
	auto strCommandPrefix = string("/api/command");
	
    if (!req.url.empty()) {
	    if (req.url.rfind(strRoutePrefix,0) == 0)
	    {
	    	auto index = strRoutePrefix.length();

	    	if (req.url[index] == '/')
                index++;
	    	
	    	auto nodeId = req.url.substr(index);

	    	if (nodeId.length() < 5)
				return req.respond_error("Node id is too short");

	    	auto firstSlash = nodeId.find("/");

	    	if (firstSlash != -1)
                nodeId = nodeId.substr(0,firstSlash);

	    	tor_route route;
	    	m_routeCreator(nodeId.c_str(),&route);
	    	auto str = route2json(&route);
	    	
	    	return req.respond("application/json", string_piece(str->c_str(),str->length()));  
		    
	    }
	    else if (req.url.rfind("/api/command",0) == 0)
		{
	    	const char* jsonRequest = req.body.c_str();

	    	auto r = jsmn_parse(&m_jsonParser,jsonRequest,strlen(jsonRequest),t, sizeof(t) / sizeof(t[0]));

            if (r < 0)

	    		return req.respond_error("Couldn't parse json");

	    	tor_command cmd;
	    	
			for (int i = 1; i < r; i++) 
			{
				if (jsoneq(jsonRequest, &t[i], "CommandBody") == 0) {
					cmd.commandBody = my_strndup(jsonRequest + t[i + 1].start,t[i + 1].end - t[i + 1].start);
					i++;
				}
				else if (jsoneq(jsonRequest, &t[i], "CommandId") == 0)
				{
					cmd.commandId = my_strndup(jsonRequest + t[i + 1].start,t[i + 1].end - t[i + 1].start);
					i++;					
				}
				else if (jsoneq(jsonRequest, &t[i], "CommandType") == 0)
				{
					cmd.commandType = my_strndup(jsonRequest + t[i + 1].start,t[i + 1].end - t[i + 1].start);
					i++;					
				}
				else if (jsoneq(jsonRequest, &t[i], "NodeId") == 0)
				{					
					cmd.nodeId = my_strndup(jsonRequest + t[i + 1].start,t[i + 1].end - t[i + 1].start);
					i++;					
				}
				else if (jsoneq(jsonRequest, &t[i], "SessionId") == 0)
				{
					cmd.sessionId = my_strndup(jsonRequest + t[i + 1].start,t[i + 1].end - t[i + 1].start);
					i++;
				}
			}
	    	 	
	    	auto code = m_commandProcessor(&cmd);
	    	
	    	return req.respond_201("application/json", "");
	    }
	    else if (req.url.rfind("/api/response",0) == 0)
		{
	    	const char* jsonRequest = req.body.c_str();

	    	auto r = jsmn_parse(&m_jsonParser,jsonRequest,strlen(jsonRequest),t, sizeof(t) / sizeof(t[0]));

	    	if (r < 0)

	    		return req.respond_error("Couldn't parse json");

	    	tor_command_replay cmd;

			for (int i = 1; i < r; i++)
			{
				if (jsoneq(jsonRequest, &t[i], "CommandResponse") == 0) {
					cmd.commandResponse = my_strndup(jsonRequest + t[i + 1].start,t[i + 1].end - t[i + 1].start);
					i++;
				}
				else if (jsoneq(jsonRequest, &t[i], "CommandId") == 0)
				{
					cmd.commandId = my_strndup(jsonRequest + t[i + 1].start,t[i + 1].end - t[i + 1].start);
					i++;
				}
				else if (jsoneq(jsonRequest, &t[i], "NodeId") == 0)
				{
					cmd.nodeId = my_strndup(jsonRequest + t[i + 1].start,t[i + 1].end - t[i + 1].start);
					i++;
				}
				else if (jsoneq(jsonRequest, &t[i], "SessionId") == 0)
				{
					cmd.sessionId = my_strndup(jsonRequest + t[i + 1].start,t[i + 1].end - t[i + 1].start);
					i++;
				}
			}

	    	auto code = m_commandProcessorReplay(&cmd);

	    	return req.respond_201("application/json", "");
	    }
	    else if (req.url.rfind("/api/paymentComplete",0) == 0)
		{
	    	const char* jsonRequest = req.body.c_str();

	    	auto r = jsmn_parse(&m_jsonParser,jsonRequest,strlen(jsonRequest),t, sizeof(t) / sizeof(t[0]));

	    	if (r < 0)

	    		return req.respond_error("Couldn't parse json");

            payment_completed cmd;

			for (int i = 1; i < r; i++)
			{
				if (jsoneq(jsonRequest, &t[i], "SessionId") == 0) {
					cmd.sessionId = my_strndup(jsonRequest + t[i + 1].start,t[i + 1].end - t[i + 1].start);
					i++;
				}
				else if (jsoneq(jsonRequest, &t[i], "Status") == 0) {
					cmd.status = atoi(my_strndup(jsonRequest + t[i + 1].start,t[i + 1].end - t[i + 1].start));
					i++;
				}
			}

	    	auto code = commandProcessingCompleted(&cmd);

	    	return req.respond("application/json", "OK");
	    }
        else
        {
        	return req.respond_not_found();
        }
    }

    return req.respond_not_found();
}