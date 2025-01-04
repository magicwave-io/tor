#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "tor_rest/rest_lib.h"

/// This function returns a route to the specified target node.
/// Parameters:
/// IN targetNode: the node id that the returned route should point to
/// OUT route: the route structure, provided as a pointer to tor_route
void getTorRoute(const char* targetNode,tor_route *route)
{
	const int routeNodeLength = 57;
	int err;

	//TODO: Add error-handling
	route->nodes = (rest_node_t *) malloc(3 * sizeof(rest_node_t));

	err = strcpy(route->nodes[0].node_id, "GDRQ2GFDIXSPOBOICRJUEVQ3JIZJOWW7BXV2VSIN4AR6H6SD32YER4LN");
	err = strcpy(route->nodes[1].node_id, "GD523N6LHPRQS3JMCXJDEF3ZENTSJLRUDUF2CU6GZTNGFWJXSF3VNDJJ");
	err = strcpy(route->nodes[2].node_id, "GB3IKDN72HFZSLY3SYE5YWULA5HG32AAKEDJTG6J6X2YKITHBDDT2PIW");	
}

int processCommand(tor_command* command)
{
		char *nodeId = command->nodeId;
}

int processCommandReplay(tor_command_replay* command)
{
		char *nodeId = command->nodeId;
}

int processPaymentCompete(payment_completed* command)
{
		char *session_id = command->sessionId;
		int status = command->status;
}


int main(int argc, char* argv[]) {
	
	if (argc < 2)
	{
	printf("Usage: [port number]");
	return 1;
	}

	int port = atoi(argv[1]);
	runServer(port,getTorRoute,processCommand, processCommandReplay, processPaymentCompete, NULL);
	getchar();

  return 0;
}
