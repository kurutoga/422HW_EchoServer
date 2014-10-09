// HW 6 - Echo server exploit
// You do not need to modify this file for this assignment

#ifndef EOFC_TCPSERVERH
#define EOFC_TCPSERVERH

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

namespace CS422
{
	// Represent a TCP communicator that connects to and communicates with another. Uses a model 
	// where all receiving of data happens internally and notifications of such events are 
	// passed through a callback function.
    class TCPComm
	{
    private:
#if defined(WIN32) || defined(_WIN32)
        TCPComm(SOCKET socket);
        
		SOCKET m_socket;

		static void Kill(SOCKET socket);

		const static SOCKET BAD_SOCKET = INVALID_SOCKET;
#else
        TCPComm(int socket);
        
        // The socket descriptor or -1 if the socket is invalid/disposed
        int m_socket;

		static void Kill(int socket);

		const static int BAD_SOCKET = -1;
#endif
        
        std::thread* m_readThread;
        
        bool m_waiting;
        
        static void ReadThreadProc(TCPComm* owner);
		
	public:
		virtual ~TCPComm();
        
        // Closes the connection. After this call no more data will be received
        // and no more data can be sent.
        void Close();
        
        // Creates the communicator by connecting to an existing server that is
        // actively listening for new connections. Returns NULL on failure.
        static TCPComm* Create(const char* serverAddress, unsigned short port);
        
        static TCPComm* CreateFromListening(unsigned short port);
        
        int Send(const void* data, int dataByteSize);

		// Stops the receiving thread for this object. Note that this does NOT close 
		// the connection. It simply kills the receiving half of the functionality.
		void StopReceiving();
        
        // Blocks the calling thread until the socket connection has been
        // severed. When a TCPComm object is created, a new thread is created
        // that continuously reads from the socket in a loop. Calling this
        // function waits for that thread to complete.
        // This function cannot be called from one of the callback functions,
        // because those are executed on the read thread. This would cause the
        // read thread to wait for itself to complete, which cannot happen. If
        // it is detected that this function is called from the read thread
        // then false will be immediately returned.
        // Also, only one call to this function can be made. If the call is made
        // more than once, then false will be returned indicating it's already
        // waiting on another call.
        // Otherwise the function will block until the read thread completes and
        // then return true.
        bool WaitForDisconnect();
        
        // A user data pointer that is set to null in the constructors and then is never 
		// used by the class afterwards.
        void* UserData;
        
        // Callback function that gets invoked when data is received. Do not block on this 
		// callback or else it will prevent data from being received.
        void (*OnDataReceived)(TCPComm* sender, unsigned char* data, int dataLength);
        
        void (*OnDisconnected)(TCPComm* sender);
	};
}

#endif