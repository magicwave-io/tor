#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "tor_rest/rest_lib.h"

int main(int argc, char* argv[]) {
	
	if (argc < 2)
	{
	printf("Usage: [port number]");
	return 1;
	}

	int port = atoi(argv[1]);
	runServer(port, NULL, NULL);
	getchar();

  return 0;
}
