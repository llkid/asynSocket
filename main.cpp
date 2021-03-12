#include <iostream>
#include <string>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <process.h>
#include <sstream>

#pragma comment(lib, "Ws2_32.lib")

SOCKET client_descs_[WSA_MAXIMUM_WAIT_EVENTS] = { INVALID_SOCKET };  //client socket����
WSAEVENT events_[WSA_MAXIMUM_WAIT_EVENTS];                     //�����¼���������
SOCKET server_desc_ = INVALID_SOCKET;                             //server socket 
WSAEVENT server_event_;                                       //server �����¼�����
int iTotal = 0;                                                //client����


/// <summary>
/// �򿪷����socket
/// </summary>
/// <param name="port">�˿ں�</param>
/// <param name="dwError">ΪdwErrorд�������</param>
/// <returns>�Ƿ�ɹ���socket</returns>
BOOL openTcpServer(_In_ u_short port, _Out_ DWORD* dwError) {
	BOOL bRet = FALSE;
	WSADATA wsaData = { 0 };
	SOCKADDR_IN server_addr = { 0 };
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

	do
	{
		if (WSAStartup(MAKEWORD(2, 2), &wsaData) == NO_ERROR) {
			if (LOBYTE(wsaData.wVersion) == 2 || HIBYTE(wsaData.wVersion) == 2) {
				server_desc_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
				if (server_desc_ != INVALID_SOCKET) {
					if (SOCKET_ERROR != bind(server_desc_, (SOCKADDR*)&server_addr, sizeof server_addr)) {
						// ���������¼�����
						server_event_ = WSACreateEvent();
						//	Ϊ server socket ע�������¼�
						WSAEventSelect(server_desc_, server_event_, FD_ACCEPT);

						if (SOCKET_ERROR != listen(server_desc_, SOMAXCONN)) {
							bRet = TRUE;
							break;
						}
						closesocket(server_desc_);
						*dwError = WSAGetLastError();
					}
					*dwError = WSAGetLastError();
					closesocket(server_desc_);
				}
				*dwError = WSAGetLastError();
			}
			*dwError = WSAGetLastError();
		}
		*dwError = WSAGetLastError();
	} while (FALSE);
	return bRet;
}


u_int __stdcall threadAccept(void* lparam) {
	WSANETWORKEVENTS networkEvents;
	while (iTotal < WSA_MAXIMUM_WAIT_EVENTS)
	{
		if (NO_ERROR == WSAEnumNetworkEvents(server_desc_, server_event_, &networkEvents)) {
			// �������FD_ACCEPT�������Ϊ1
			if (networkEvents.lNetworkEvents & FD_ACCEPT) {
				int code = networkEvents.iErrorCode[FD_ACCEPT_BIT];
				// ��������������
				if (NO_ERROR == code) {
					//��������
					SOCKADDR_IN addrServer = { 0 };
					int iaddrLen = sizeof(addrServer);
					client_descs_[iTotal] = accept(server_desc_, (SOCKADDR*)&addrServer, &iaddrLen);
					if (client_descs_[iTotal] == INVALID_SOCKET) {
						wprintf(L"accept failed with error code: %d\n", WSAGetLastError());
						return 1;
					}
					//Ϊ�µ�clientע�������¼�
					events_[iTotal] = WSACreateEvent();
					WSAEventSelect(client_descs_[iTotal], events_[iTotal], FD_READ | FD_WRITE | FD_CLOSE);
					++iTotal;
					char remote_addr[32] = {0};
					inet_ntop(AF_INET, &addrServer.sin_addr, remote_addr, 32);
					printf("accept a connection from IP: %s,Port: %d\n", remote_addr, htons(addrServer.sin_port));
				}
				// ������
				else {
					wprintf(L"WSAEnumNetworkEvents failed with error code: %d\n", code);
					return 1;
				}
			}
		}
		Sleep(100);
	}
	return 0;
}

// __stdcall ��ʾԼ�����ڵ��� Win32 api����
// �����������մ��ҵ����˳����ջ
// �������밴���β��������
u_int __stdcall threadRecv(void* lpram) {
	char* buf = new char[sizeof(char) * 128]();
	while (true)
	{
		if (!iTotal) {
			Sleep(100);
			continue;
		}

		// �ȴ��������
		DWORD dwIndex = WSAWaitForMultipleEvents(iTotal, events_, FALSE, 1000, FALSE);
		// ��ǰ���¼�����
		WSAEVENT curEvent = events_[dwIndex];
		// ��ǰ�ͻ����׽���
		SOCKET curSocket = client_descs_[dwIndex];
		// �����¼��ṹ
		WSANETWORKEVENTS networkEvent;
		if (NO_ERROR == WSAEnumNetworkEvents(curSocket, curEvent, &networkEvent)) {
			if (networkEvent.lNetworkEvents & FD_READ) {
				int code = networkEvent.iErrorCode[FD_READ_BIT];
				if (NO_ERROR == code) {
					memset(buf, 0, sizeof(buf));
					int iRet = recv(curSocket, buf, sizeof(buf), 0);
					if (iRet != SOCKET_ERROR) {
						std::wstringstream wss;
						wss << buf;
						wprintf(L"Recv: %s\n", wss.str().c_str());
					}
				}
				else {
					wprintf(L"WSAEnumNetworkEvents failed with error: %d\n", code);
					break;
				}
			}
			else if (networkEvent.lNetworkEvents & FD_CLOSE) {
				wprintf(L"%d shutdown\n", curSocket);
			}
		}
		Sleep(100);
	}
	if (buf) {
		delete[] buf;
		buf = nullptr;
	}
	return 0;
}

int main(int argc, char** argv)
{
	DWORD dwError = 0;
	if (openTcpServer(18000, &dwError)) {
		_beginthreadex(NULL, 0, threadAccept, NULL, 0, NULL);
		_beginthreadex(NULL, 0, threadRecv, NULL, 0, NULL);
	}
	Sleep(100000000);
	closesocket(server_desc_);
	WSACleanup();

	return 0;
}