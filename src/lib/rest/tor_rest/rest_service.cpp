#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include <string.h>
#include <stdio.h>

#include "microrestd.h"
#include "rest_lib.h"
#include "tor_rest_service.h"
#include <sstream>

#include "rest_lib.h"

using namespace std;
using namespace ufal::microrestd;


tor_rest_service::tor_rest_service(
		void (*log_function)(const char *message),
		int (*handler)(tor_http_api_request_t *) /* = nullptr*/
	)
{
	m_log_handler = log_function;
	m_handler = handler;
}

void tor_rest_service::log(const char *message)
{
	if(m_log_handler)
		m_log_handler(message);
}

void tor_rest_service::req_log(rest_request& req)
{
	if(!m_log_handler)
		return;

	std::stringstream message;
	message << "===[ tor_rest_service::req_log ]=================================" << std::endl;
	message << "method: " << req.method << std::endl;
	message << "url: " << req.url << std::endl;
	message << "content_type: " << req.content_type << std::endl;
	message << "body_len: " << req.body.size() << std::endl;
	message << "body: " << req.body << std::endl;
	message << "=================================================================";
	std::string tmp = message.str();
	m_log_handler(tmp.c_str());
}

bool tor_rest_service::handle(rest_request& req)
{
	req_log(req);
    if (req.method != "HEAD" && req.method != "GET" && req.method != "POST") 
        return req.respond_method_not_allowed("HEAD, GET, POST");

    if (!req.url.empty()) {
		if (NULL != m_handler) {
			tor_http_api_request_t request;
			std::memset(&request, 0, sizeof(request));
			request.method = req.method.c_str();
			request.url = req.url.c_str();
			request.body = req.body.c_str();
			vector<tor_http_api_request_param_t> params;
			if(req.params.size()) {
				request.param_count = req.params.size();
				params.resize(request.param_count);
				request.params = params.data();
				size_t i = 0;
				for (auto& p : req.params) {
					request.params[i].name = p.first.c_str();
					request.params[i].value = p.second.c_str();
					i++;
				}
			}
			int rc =  m_handler(&request);
			switch (rc){
			case TOR_HTTP_RESULT_OK:
				if (request.answer_body)
					return req.respond(request.answer_plain_text ? "text/plain": "application/json", request.answer_body);
				else
					return req.respond_201("application/json", "");
			case TOR_HTTP_RESULT_WRONG_METHOD:
				return req.respond("application/json", "{\"result\":\"wrong method\"}");
			case TOR_HTTP_RESULT_WRONG_URL:
				return req.respond("application/json", "{\"result\":\"wrong url\"}");
			case TOR_HTTP_RESULT_WRONG_BODY:
				return req.respond("application/json", "{\"result\":\"wrong body\"}");
			case TOR_HTTP_RESULT_WRONG_JSON:
				return req.respond("application/json", "{\"result\":\"wrong json\"}");
			case TOR_HTTP_RESULT_WRONG_PARAMETER:
				return req.respond("application/json", "{\"result\":\"wrong parameter\"}");
			case TOR_HTTP_RESULT_UNKNOWN:
			default:
				return req.respond("application/json", "{\"result\":\"unknown\"}");
			}
			if(request.release && request.answer_body) {
				request.release(request.answer_body);
			}
		}
        else
        {
        	return req.respond_not_found();
        }
    }

    return req.respond_not_found();
}