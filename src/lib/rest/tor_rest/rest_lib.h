#include <stdbool.h>

#define CONST_ROUTE_NODE_ID_LENGTH 100

typedef struct rest_node_t {
    char* node_id;
    char* address;
} rest_node_t;

typedef struct tor_route {
    rest_node_t* nodes;
    int nodes_len;
    char* call_back_url; 		        // process command url
    char* status_call_back_url; 		// process command url
} tor_route;

typedef struct tor_command {
    char * commandBody;
    char * commandId;
    char * commandType;
    char * nodeId;
    char * sessionId;
    char * json_body;
} tor_command;

typedef struct payment_completed {
    char * sessionId;
    int status;
    char * json_body;
} payment_completed;


typedef struct tor_command_replay {
    char * commandResponse;
    char * commandId;
    char * nodeId;
    char * sessionId;
    char * json_body;
} tor_command_replay;

#ifdef __cplusplus
extern "C" {
#endif

int runServer(int port, void (*routeFunction)(const char* targetNode, tor_route *route),
              int (*commandProcessingFunction)(tor_command *command),
              int (*commandProcessingReplayFunction)(tor_command_replay *command),
              int (*commandProcessingCompletedFunction)(payment_completed *command),
              const char *appVersionString /*= NULL*/);

#ifdef __cplusplus
}
#endif