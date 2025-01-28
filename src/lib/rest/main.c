#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "tor_rest/rest_lib.h"

/// This function returns a route to the specified target node.
/// Parameters:
/// IN targetNode: the node id that the returned route should point to
/// OUT route: the route structure, provided as a pointer to tor_route
void getTorRoute(tor_route *route)
{
	const int routeNodeLength = 57;
	int err;

	//TODO: Add error-handling
	rest_node_t* nodes = (rest_node_t *) malloc(3 * sizeof(rest_node_t));
	route->nodes = nodes;

	strcpy(nodes[0].node_id, "NODE1");
	strcpy(nodes[0].address, "GDRQ2GFDIXSPOBOICRJUEVQ3JIZJOWW7BXV2VSIN4AR6H6SD32YER4LN");
	strcpy(nodes[1].node_id, "NODE2");
	strcpy(nodes[1].address, "GD523N6LHPRQS3JMCXJDEF3ZENTSJLRUDUF2CU6GZTNGFWJXSF3VNDJJ");
	strcpy(nodes[2].node_id, "NODE3");
	strcpy(nodes[2].address, "GB3IKDN72HFZSLY3SYE5YWULA5HG32AAKEDJTG6J6X2YKITHBDDT2PIW");	
}

int processCommand(tor_command* command)
{
	const char *nodeId = command->nodeId;
}

int processCommandReplay(tor_command_replay* command)
{
	const char *nodeId = command->nodeId;
}

int processPaymentCompete(payment_completed* command)
{
	const char *session_id = command->sessionId;
	int status = command->status;
}


int main(int argc, char* argv[]) {
	
	if (argc < 2)
	{
	printf("Usage: [port number]");
	return 1;
	}

	int port = atoi(argv[1]);
	runServer(port,getTorRoute,processCommand, processCommandReplay, processPaymentCompete, NULL, NULL);
	getchar();

  return 0;
}
