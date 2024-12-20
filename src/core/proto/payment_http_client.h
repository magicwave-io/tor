#ifndef __PAYMENT_HTTP_CLIENT_H__INCLUDED__
#define __PAYMENT_HTTP_CLIENT_H__INCLUDED__

#include "trunnel/circpad_negotiation.h"
#include "lib/evloop/timers.h"
#include <json-c/json.h>
#include <src/core/or/origin_circuit_st.h>

typedef struct  routing_node {
    char node_id[MAX_NICKNAME_LEN + 1];
    char address[STELLAR_ADDRESS_LEN + 1];
} routing_node_t;

typedef struct create_payment_info {
    const char* service_type;
    const char* commodity_type;
    int amount;
} create_payment_info_t;

typedef struct process_payment_request {
    routing_node_t* routing_node;
    char* call_back_url; 		        // process command url
    char* status_call_back_url; 		        // process command url
    char* payment_request;	        // json body
    char* node_id;           		// request reference identification
    char* session_id;

} process_payment_request_t;

typedef struct utility_command{
    int command_type;
    char* command_body;
    char* callback_url;
    char* command_id;
    char* node_id;
    char* session_id;
} utility_command_t;

typedef struct utility_response{
    char* command_id;
    char* response_body;
    char* node_id;
    char* session_id;
} utility_response_t;

char* tp_create_payment_info(char *url, create_payment_info_t* request); //1
void tp_http_payment(char *url, process_payment_request_t* request, size_t hup_num); //2
void tp_http_command(char *url, utility_command_t* request); //4
void tp_http_response(char *url, utility_response_t* request); //5
void ship_log(const char * prefix, const char *url, const char* requestBody, const char* responseBody);
json_object* tp_http_get_request(const char* url_input);
char* tp_http_post_request(const char* url_input, const char* json);

#define PAYMENT_REQUEST "request"
#define PAYMENT_CALLBACK "callback"

#endif // __PAYMENT_HTTP_CLIENT_H__INCLUDED__
