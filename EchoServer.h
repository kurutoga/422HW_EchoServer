// HW 6 - Echo server exploit
// Do NOT modify this file for this assignment
// This is part of the server code, which cannot be changed

// Platform-specific includes:
#if defined(WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <stdio.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#endif

// Standard C/C++ includes:
#include <thread>
#include <string.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <mutex>

#include "Types.h"

// Structure representing a client that is connected to the server
struct Client
{
	char Buffer[1024];

	// 1 = structure represents a connected client and is in use
	// 0 = structure is in the pool to be reused for a new connection
	volatile int InUse;

	int PoolIndex;

#if defined(WIN32) || defined(_WIN32)
	SOCKET Socket;
#else
	int Socket;
#endif
};