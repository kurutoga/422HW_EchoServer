// HW 6 - Echo server exploit
// Do NOT modify this file for this assignment
// This is part of the server code, which cannot be changed

#include "EchoServer.h"
#include "MemoryStream.h"

using namespace std;
using namespace CS422;

#if defined(WIN32) || defined(_WIN32)
void Kill(SOCKET& socket);
const static SOCKET BAD_SOCKET = INVALID_SOCKET;
#else
void Kill(int& socket);
const static int BAD_SOCKET = -1;
#endif

void ClientThreadFunc(Client* client);

// Pool of clients
Client clients[10];

// On Windows we'll need WinSock
#if defined(WIN32) || defined(_WIN32)
WSADATA wsaData;
#endif

int main(int argc, const char* argv[])
{
	// Initialize the client structures
	int clientCount = sizeof(clients) / sizeof(Client);
	for (int i = 0; i < clientCount; i++)
	{
		strcpy(clients[i].Buffer, "welcome.txt");
		clients[i].InUse = 0;
		clients[i].PoolIndex = i;
		clients[i].Socket = BAD_SOCKET;
	}

#if defined(WIN32) || defined(_WIN32)
	// Initialize Winsock if we're on Windows
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		cout << "WSAStartup failed!" << endl;
		return - 1;
	}
#endif

	int port = 42200;
	
	// ---- Setup a server that listens for a connection ----
	cout << "Listening for a connection on port " << port << endl;

	// First create the socket for listening for connections
	auto listener = socket(AF_INET, SOCK_STREAM, 0);

	// If BAD_SOCKET was returned then it failed
	if (BAD_SOCKET == listener)
	{
#if defined(WIN32) || defined(_WIN32)
		WSACleanup();
#endif
		return NULL;
	}

	sockaddr_in serv_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);
	if (::bind(listener, (sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
	{
		return NULL;
	}

	// Start listening
	if (BAD_SOCKET == listen(listener, 5))
	{
		return NULL;
	}

	while (true)
	{
		// Make the blocking call to accept the socket
		auto socket = accept(listener, NULL, NULL);

		if (BAD_SOCKET == socket)
		{
			cout << "Could not get socket from accept(...) call" << endl;
			return -1;
		}

		int i = 0;
		while (i < clientCount && 1 == clients[i].InUse)
		{
			i++;
		}

		if (i == clientCount)
		{
			cout << "No clients available to handle request -> dropping" << endl;
			Kill(socket);
			continue;
		}

		// Create a new client from this connection
		strcpy(clients[i].Buffer, "welcome.txt");
		clients[i].InUse = 1;
		clients[i].PoolIndex = 0;
		clients[i].Socket = socket;

		cout << "Server allows new socket connection at pool index " << i << endl;

		// Create a new thread to handle this client
		thread t(ClientThreadFunc, &clients[i]);
		// Let the thread run on its own
		t.detach();
	}

	// Stop listening
	Kill(listener);

#if defined(WIN32) || defined(_WIN32)
	WSACleanup();
#endif

	cout << "Done" << endl;
	return 0;
}

void ClientThreadFunc(Client* client)
{
	cout << "Server in ClientThreadFunc, will now wait for handshake" << endl;
	
	// Read the handshake message
	char handshake_msg[1024];
	memset(handshake_msg, 0, sizeof(handshake_msg));
#if defined(WIN32) || defined(_WIN32)
	auto hsByteCount = recv(client->Socket, handshake_msg, sizeof(handshake_msg) - 1, 0);
#else
	auto hsByteCount = read(socket, handshake_msg, sizeof(handshake_msg) - 1);
#endif
	if (0 != strcmp(handshake_msg, "422 echo server handshake"))
	{
		// Not a valid client
		Kill(client->Socket);
		client->InUse = 0;
		cout << "Client handshake was incorrect -> connection refused" << endl;
		cout << "  Client sent:" << endl;
		cout << "  " << handshake_msg << endl;
		return;
	}

	cout << "New client connected!" << endl;
	auto Socket = client->Socket;
	int bufSize = sizeof(client->Buffer);
	
	// Send the welcome message
	cout << "Server about to open welcome file: " << client->Buffer << endl;
	FILE* welcome = fopen(client->Buffer, "rb");
	if (!welcome)
	{
		cout << "Could not open welcome file!" << endl;
		// Shut down the socket
		Kill(client->Socket);
		client->Socket = BAD_SOCKET;
		// Mark this client structure as available for use
		client->InUse = 0;
		cout << "Client disconnected" << endl;
		return;
	}
	fseek(welcome, 0, SEEK_END);
	auto welcomeFileSize = ftell(welcome);
	fseek(welcome, 0, SEEK_SET);
	memset(client->Buffer, 0, sizeof(client->Buffer));
	fread(client->Buffer, 1, welcomeFileSize, welcome);
	fclose(welcome);
#if defined(WIN32) || defined(_WIN32)
	send(Socket, client->Buffer, welcomeFileSize, 0);
#else
	write(Socket, client->Buffer, welcomeFileSize);
#endif

	// Now that we have a socket, enter the read loop
	while (true)
	{
		MemoryStream ms;		
		// Receive into the client buffer
#if defined(WIN32) || defined(_WIN32)
		int bytesRead = recv(Socket, client->Buffer, bufSize, 0);
#else
		int bytesRead = read(Socket, client->Buffer, bufSize);
#endif
		
		// Break the loop if we have an error or no data
		if (bytesRead <= 0) { break; }

		// Append to memory stream
		ms.Write(&client->Buffer[2], bytesRead - 2);
		// First 16-bits is unsigned short telling us total message size in bytes
		int msgSize = ((u16*)&client->Buffer[0])[0];
		// If we didn't receive all the data then do additional receives
		while (bytesRead < msgSize)
		{
#if defined(WIN32) || defined(_WIN32)
			int read = recv(Socket, client->Buffer, bufSize, 0);
#else
			int read += read(Socket, client->Buffer, bufSize);
#endif
			bytesRead += read;
			// Append to memory stream
			ms.Write(client->Buffer, read);
		}
		int zero = 0;
		ms.Write(&zero, 1);
		
		char* fullMsg = (char*)ms.GetPointer();
		cout << "Server received (and will echo back): " << fullMsg << endl;
		// Echo back
#if defined(WIN32) || defined(_WIN32)
		send(Socket, fullMsg, strlen(fullMsg), 0);
#else
		write(Socket, fullMsg, strlen(fullMsg));
#endif
		// Clear buffer
		memset(client->Buffer, 0, strlen(client->Buffer));
	}

	// Shut down the socket
	Kill(client->Socket);
	client->Socket = BAD_SOCKET;

	// Mark this client structure as available for use
	client->InUse = 0;

	cout << "Client disconnected" << endl;
}

#if defined(WIN32) || defined(_WIN32)
void Kill(SOCKET& socket)
{
	if (INVALID_SOCKET != socket)
	{
		shutdown(socket, SD_BOTH);
		closesocket(socket);
		socket = INVALID_SOCKET;
	}
}
#else
void Kill(int& socket)
{
	shutdown(socket, SHUT_RDWR);
	close(socket);
	socket = -1;
}
#endif