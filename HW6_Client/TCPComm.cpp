// HW 6 - Echo server exploit
// You do not need to modify this file for this assignment

#include "TCPComm.h"

// On Windows we'll need WinSock
#if defined(WIN32) || defined(_WIN32)
WSADATA wsaData;
#endif

namespace CS422
{
#if defined(WIN32) || defined(_WIN32)
    TCPComm::TCPComm(SOCKET socket)
    {
		// Initialize Winsock
        int iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
        if (iResult != 0)
        {
            // TODO: Set an error message
            throw std::exception("WSAStartup failed!");
            return;
        }
#else
    TCPComm::TCPComm(int socket)
    {
#endif
        m_socket = socket;
        OnDataReceived = NULL;
        OnDisconnected = NULL;
        m_waiting = false;
        UserData = NULL;
        
        // Create a thread for reading
        m_readThread = new std::thread(ReadThreadProc, this);
    }
    
    TCPComm::~TCPComm()
    {
        Close();

#if defined(WIN32) || defined(_WIN32)
		WSACleanup();
#endif
    }

    void TCPComm::Close()
    {
#if defined(WIN32) || defined(_WIN32)
		// Start by shutting down the socket. If the read thread is running and
		// blocked in a "read" call, then this will abort it, causing read to
		// return and break the loop in the thread.
		if (INVALID_SOCKET != m_socket)
		{
			shutdown(m_socket, SD_BOTH);
			closesocket(m_socket);
			m_socket = INVALID_SOCKET;
		}
#else
		// Start by shutting down the socket. If the read thread is running and
		// blocked in a "read" call, then this will abort it, causing read to
		// return -1 which breaks the loop in the thread.
		if (-1 != m_socket)
        {
            shutdown(m_socket, SHUT_RDWR);
			close(m_socket);
            m_socket = -1;
        }
#endif
        
        // If the thread object is already NULL then we're done
        if (!m_readThread) { return; }
        
        // If we're waiting for disconnect, then we've already performed a join
        // on the read thread. In this case we just return and rely on the
        // object's destructor to free the thread later.
        if (m_waiting) { return; }
        
        // With the read loop breaking, the read thread will soon end. We need
        // to wait for it to complete (join) then free it.
        // The only exception to this is if we're getting closed from the read
        // thread itself, in which case we have to detach the thread and let
        // it complete on its own.
        if (std::this_thread::get_id() == m_readThread->get_id())
        {
            m_readThread->detach();
        }
        else
        {
            if (m_readThread->joinable())
            {
                m_readThread->join();
            }
        }
        delete m_readThread;
        m_readThread = NULL;
    }
    
	TCPComm* TCPComm::Create(const char* serverAddress, unsigned short port)
    {
#if defined(WIN32) || defined(_WIN32)
		// On Windows we need to initialize Winsock
		int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != 0)
		{
			throw std::exception("WSAStartup failed!");
			return NULL;
		}
#endif
		
		// First create the socket
        auto sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock == BAD_SOCKET)
        {
            return NULL;
        }
        
        // Resolve the server address and connect
        struct addrinfo *result;
		char portStr[64];
		itoa((int)port, portStr, 10);
        struct addrinfo hints;
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET;
        if (0 != getaddrinfo(serverAddress, portStr, &hints, &result))
        {
			Kill(sock);
            return NULL;
        }
        if (connect(sock, result->ai_addr, result->ai_addrlen) < 0)
        {
            Kill(sock);
            return NULL;
        }
        freeaddrinfo(result);
        
        return new TCPComm(sock);
    }
    
    // Starts listening on the specified port until a connection is made and then
    // returns the communicator. Returns NULL on failure.
    TCPComm* TCPComm::CreateFromListening(unsigned short port)
    {
#if defined(WIN32) || defined(_WIN32)
		// On Windows we need to initialize Winsock
		int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
		if (iResult != 0)
		{
			throw std::exception("WSAStartup failed!");
			return NULL;
		}
#endif
		
		// First create the socket for listening for connections
        int listener = socket(AF_INET, SOCK_STREAM, 0);
        
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
        
        // Make the blocking call to accept the socket
        int socket = accept(listener, NULL, NULL);
            
        TCPComm* comm = NULL;
		if (BAD_SOCKET != socket)
        {
            comm = new TCPComm(socket);
        }
        
        // Stop listening
		Kill(listener);
        
        return comm;
    }

#if defined(WIN32) || defined(_WIN32)
	void TCPComm::Kill(SOCKET socket)
	{
		shutdown(socket, SD_BOTH);
		closesocket(socket);
	}
#else
	void TCPComm::Kill(int socket)
	{
		shutdown(socket, SHUT_RDWR);
		close(socket);
	}
#endif
    
    void TCPComm::ReadThreadProc(TCPComm* owner)
    {
        unsigned char buf[4096];
		while (-1 != owner->m_socket)
        {
#if defined(WIN32) || defined(_WIN32)
			auto bytesRead = recv(owner->m_socket, (char*)buf, sizeof(buf)-1, 0);
#else
			auto bytesRead = read(owner->m_socket, buf, sizeof(buf)-1);
#endif
            
            // Break the loop if we have an error or no data
            if (bytesRead <= 0)
            {
                break;
            }
            
            // Invoke the callback for data
            if (owner->OnDataReceived)
            {
                owner->OnDataReceived(owner, buf, (int)bytesRead);
            }
        }
        
        // When we break the read loop, it implies that we
        // are disconnecting. Therefore we free resources
        // and invoke the disconnected event.
        owner->Close();
        if (owner->OnDisconnected)
        {
            owner->OnDisconnected(owner);
        }
    }
    
#if defined(WIN32) || defined(_WIN32)
    int TCPComm::Send(const void* data, int dataByteSize)
    {
		if (INVALID_SOCKET == m_socket) { return -1; }
		return (int)send(m_socket, (const char*)data, dataByteSize, 0);
    }
#else
    int TCPComm::Send(const void* data, int dataByteSize)
    {
        if (-1 == m_socket) { return -1; }
        return (int)write(m_socket, data, dataByteSize);
    }
#endif
    
    bool TCPComm::WaitForDisconnect()
    {
        // If we're already waiting then we can't wait again
        if (m_waiting)
        {
            return false;
        }
        
        // If the read thread is NULL it means we're already disconnected (or in
        // the process of doing so) and we immediately return true.
        if (!m_readThread)
        {
            return true;
        }
        if (std::this_thread::get_id() == m_readThread->get_id())
        {
            return false;
        }
        
        m_waiting = true;
        m_readThread->join();
        m_waiting = false;
        return true;
    }

} // end of EOFC namespace