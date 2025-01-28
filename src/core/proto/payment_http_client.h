#include "trunnel/circpad_negotiation.h"
#include "lib/evloop/timers.h"
#include <json-c/json.h>
#include <src/core/or/origin_circuit_st.h>

typedef struct log_args_st {
    char *url;
    char* requestBody;
    char* responseBody;
} log_args_t;

typedef struct  routing_node {
    char* node_id;
    char* address;
} routing_node_t;

typedef struct create_payment_info {
    char* service_type;
    char* commodity_type;
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


typedef struct payment_response_t {
    char* response_body;

} payment_response_t;

typedef struct stellar_address_response_t {
    char* address;

} stellar_address_response_t;

//public API

int
circuit_get_num_by_nickname(origin_circuit_t *circ, char* nickname);

char*
create_payment_info(char *url, create_payment_info_t* request); //1
payment_response_t*
process_payment(char *url, process_payment_request_t* request, int hup_num); //2
payment_response_t*
process_command(char *url, utility_command_t* request); //4
payment_response_t*
process_response(char *url, utility_response_t* request); //5
stellar_address_response_t* get_stellar_address(char *url);
void ship_log(log_args_t* args);
char* send_http_post_request(const char* url_input, const char* json);
json_object* send_http_get_request(const char* url_input);