#include <ws2tcpip.h>
#include <Windows.h>
#include <string>
#include <iostream>

#include "server.h"

#pragma comment(lib, "ws2_32.lib")

void EndProcess()
{
	std::cin.get();
	ExitProcess(0);
	return;
}

int main()
{
	SetConsoleTitleA("Donetsk Master Server");

	if (!initWSA()) EndProcess();

	if (!initServerSocket()) EndProcess();

	if (!bindServerSocket()) EndProcess();

	puts("Starting server...");

	CreateThread(0, 0, (LPTHREAD_START_ROUTINE)recvThread, 0, 0, 0);
	sendThread();

	while (recvThreadStillRunning());

	puts("Stopped server.");

	return 0;
}