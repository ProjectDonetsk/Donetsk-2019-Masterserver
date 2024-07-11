#pragma once

bool initWSA();

bool initServerSocket();

bool bindServerSocket();

bool recvThreadStillRunning();

void recvThread();

void sendThread();