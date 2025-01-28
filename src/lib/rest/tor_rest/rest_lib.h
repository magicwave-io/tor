#include <stdbool.h>
#include <stdlib.h>

#ifndef MAX_HEX_NICKNAME_LEN
    #define HEX_DIGEST_LEN_ 40
    #define MAX_HEX_NICKNAME_LEN (HEX_DIGEST_LEN_+1)
#endif
#ifndef STELLAR_ADDRESS_LEN
    #define STELLAR_ADDRESS_LEN 100
#endif

typedef struct rest_node_t {
    char node_id[MAX_HEX_NICKNAME_LEN+1];
    char address[STELLAR_ADDRESS_LEN];
} rest_node_t;

typedef struct tor_route {
    rest_node_t* nodes;
    size_t nodes_len;
    const char* call_back_url; 		        // process command url
    const char* status_call_back_url; 		// process command url
} tor_route;

typedef struct tor_command {
    const char * commandBody;
    const char * commandId;
    const char * commandType;
    const char * nodeId;
    const char * sessionId;
    const char * json_body;
} tor_command;

typedef struct payment_completed {
    const char * sessionId;
    int status;
    const char * json_body;
} payment_completed;

typedef struct tor_command_replay {
    const char * commandResponse;
    const char * commandId;
    const char * nodeId;
    const char * sessionId;
    const char * json_body;
    const char * commandType;
} tor_command_replay;

#ifdef __cplusplus
extern "C" {
#endif

int runServer(
        int port,
        void (*routeFunction)(const char* targetNode, tor_route *route),
        int (*commandProcessingFunction)(tor_command *command),
        int (*commandProcessingReplayFunction)(tor_command_replay *command),
        int (*commandProcessingCompletedFunction)(payment_completed *command),
        void (*log_function)(const char *message),
        const char *appVersionString /*= NULL*/
    );
int stopServer(void);

#ifdef __cplusplus
}
#endif
