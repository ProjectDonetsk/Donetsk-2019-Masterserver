#include <ws2tcpip.h>
#include <Windows.h>
#include <string>
#include <iostream>

#include "server.h"

enum NetCommands
{
	CMD_ADD_CLIENT = 0x1234,
	CMD_REMOVE_CLIENT = 0x1235,
	CMD_GET_IPS = 0x1236,
	CMD_KILL_SERVER = 0x1237
};

enum NetResponses
{
	RES_IPS = 0x2345,
	RES_STILL_THERE = 0x2346,
};

void sayIP(const char* prefix, unsigned int ip)
{
	char ipAddress[22];
	inet_ntop(AF_INET, &ip, ipAddress, 22);
	printf("%s %s\n", prefix, ipAddress);
}

const int maxIPs = 1500;
unsigned int clientIPs[maxIPs] = { NULL };
struct sockaddr_in clients[maxIPs] = { NULL };

unsigned int isIPAlreadyConnected(unsigned int ip)
{
	for (int i = 0; i < maxIPs; i++)
	{
		if (clientIPs[i] == ip)
		{
			return i;
		}
	}
	return -1;
}

void addIP(unsigned int ip)
{
	for (int i = 0; i < maxIPs; i++)
	{
		if (clientIPs[i] == NULL)
		{
			clientIPs[i] = ip;
			puts("DEBUG: added IP to list");
			return;
		}
	}
}

void removeIP(unsigned int ip)
{
	for (int i = 0; i < maxIPs; i++)
	{
		if (clientIPs[i] == ip)
		{
			clientIPs[i] = NULL;
			puts("DEBUG: removed IP from list");
			return;
		}
	}
}

WSAData wsaData;
SOCKET serverSock;
struct sockaddr_in server;
struct sockaddr_in lastClient;
bool recvThreadRunning = false;
bool stopServer = false;

bool initWSA()
{
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != NULL)
	{
		printf("WSAStartup error: %lu\n", WSAGetLastError());
		return false;
	}
	puts("WSA initalized.");
	
	return true;
}

bool initServerSocket()
{
	memset(&serverSock, NULL, sizeof(serverSock));

	serverSock = socket(AF_INET, SOCK_DGRAM, 0); // serverSock = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSock == INVALID_SOCKET)
	{
		printf("Error creating socket: %lu\n", WSAGetLastError());
		return false;
	}
	puts("Server socket created.");
	return true;
}

bool bindServerSocket()
{
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(5000); // 26730

	BOOL yes = TRUE;
	if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes)) == SOCKET_ERROR)
	{
		printf("setsockopt error: %lu\n", WSAGetLastError());
		return false;
	}

	if (bind(serverSock, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR)
	{
		printf("Error binding socket: %lu\n", WSAGetLastError());
		return false;
	}

	puts("Bound server socket.");
	return true;
}

bool recvThreadStillRunning()
{
	return recvThreadRunning;
}

void recvThread()
{
	recvThreadRunning = true;
	puts("Started recvThread...");

	const int maxMsgSize = 2048;
	char message[maxMsgSize];

	struct sockaddr_in client;
	static int clientLen;
	clientLen = sizeof(client);

	memset(&lastClient, NULL, sizeof(lastClient));

	while (stopServer == false)
	{
		int msgLen = NULL;
		if ((msgLen = recvfrom(serverSock, message, maxMsgSize, 0, (struct sockaddr*)&client, &clientLen)) == SOCKET_ERROR)
		{
			DWORD errorNum = WSAGetLastError();
			if (errorNum == 10054)
			{
				removeIP(client.sin_addr.S_un.S_addr);
			}
			else
			{
				printf("Warning: recvfrom error %lu\n", errorNum);
			}
		}
		else
		{
			int msgValue;
			memcpy(&msgValue, message, 4);
			unsigned int clientIP = client.sin_addr.S_un.S_addr;

			if (msgValue == CMD_ADD_CLIENT)
			{
				if (isIPAlreadyConnected(clientIP) == -1)
				{
					addIP(clientIP);
					memcpy(&lastClient, &client, sizeof(client));
				}
				unsigned int msg = isIPAlreadyConnected(clientIP);
				memcpy(&clients[msg], &client, sizeof(client));
				sendto(serverSock, reinterpret_cast<const char*>(&msg), sizeof(msg), 0, (struct sockaddr*)&client, sizeof(client));
			}

			if (msgValue == CMD_REMOVE_CLIENT)
			{
				removeIP(clientIP); // removes ip only if it's already existing in the array
			}

			if (msgValue == CMD_GET_IPS)
			{
				const int intLen = sizeof(int);
				unsigned int sendMsg[maxIPs + 1];
				sendMsg[0] = RES_IPS;
				
				static unsigned int tempIPs[maxIPs];
				unsigned int addedIPs = 0;
				memset(tempIPs, 0, maxIPs * intLen);
				for (int i = 0; i < maxIPs; i++)
				{
					if (clientIPs[i] != NULL)
					{
						tempIPs[addedIPs] = clientIPs[i];
						addedIPs++;
					}
				}
				// memcpy(sendMsg + intLen, tempIPs, addedIPs * intLen);
				unsigned int msgLen = addedIPs * intLen + intLen;
				unsigned int ipsInList = (msgLen - 4) / 4;
				struct ipResp
				{
					unsigned int msg;
					unsigned int ips[];
				};

				ipResp* ipList = reinterpret_cast<ipResp*>(sendMsg);

				ipList->msg = RES_IPS;
				for (int i = 0; i < ipsInList; i++)
				{
					ipList->ips[i] = tempIPs[i];
				}

				sendto(serverSock, (const char*)sendMsg, msgLen, 0, (struct sockaddr*)&client, sizeof(client));
			}

			if (msgValue == CMD_KILL_SERVER)
			{
				if (msgLen > 4)
				{
					if (strcmp(message + 4, "ThisIsPasswordToKillServer_lol") == 0)
					{
						stopServer = true;
					}
				}
			}
		}
	}
	recvThreadRunning = false;
}

void sendThread()
{
	puts("Started sendThread...");

	while (stopServer == false)
	{
		Sleep(5000);

		for (int i = 0; i < maxIPs; i++)
		{
			if (clientIPs[i] != NULL)
			{
				unsigned int msg = RES_STILL_THERE;
				struct sockaddr_in client;
				memcpy(&client, &clients[i], sizeof(client));

				if (sendto(serverSock, reinterpret_cast<const char*>(&msg), sizeof(msg), 0, (struct sockaddr*)&client, sizeof(client)) == SOCKET_ERROR)
				{
					removeIP(clientIPs[i]);
					DWORD errorNum = WSAGetLastError();
					if (errorNum != 10054)
					{
						printf("Sending client RES_STILL_THERE resulted in an unexpected error from WSA (%lu)\n", errorNum);
					}
					puts("DEBUG: sending client RES_STILL_THERE resulted in SOCKET_ERROR");
				}
				// else: client is fine
			}
		}
	}

	puts("stopping server...");

	closesocket(serverSock);
}