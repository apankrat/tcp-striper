#include "socket_utils.h"

#include <stdio.h>

const char * sa_to_str(const sockaddr_in * sa, char * buf, size_t max)
{
	uint32_t addr = htonl( SOCKADDR_IN_ADDR(sa) );
	uint16_t port = htons( SOCKADDR_IN_PORT(sa) );
	snprintf(buf, max, "%u.%u.%u.%u:%hu",
		(addr >>  0) & 0xFF,(addr >>  8) & 0xFF,
		(addr >> 16) & 0xFF,(addr >> 24) & 0xFF, port);

	return buf;
}

