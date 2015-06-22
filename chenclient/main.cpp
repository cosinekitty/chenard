#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdlib.h>
#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>

int main(int argc, const char *argv[])
{
    if (argc != 4)
    {
        std::cerr << "USAGE:  chenclient server port \"message\"" << std::endl;
        return 1;
    }

    const char *server = argv[1];
    int port = atoi(argv[2]);
    const char *message = argv[3];

    if (port <= 0 || port > 0xffff)
    {
        std::cerr << "ERROR: Invalid port number '" << argv[2] << "'" << std::endl;
        return 1;
    }

    WSADATA data;
    const WORD versionRequested = MAKEWORD(2, 2);
    int result = WSAStartup(versionRequested, &data);
    if (result == 0)
    {
        if (data.wVersion == versionRequested)
        {
            hostent *host = gethostbyname(server);
            if (host == nullptr)
            {
                std::cerr << "ERROR: Cannot resolve host '" << server << "'" << std::endl;
            }
            else
            {
                const char *iptext = inet_ntoa(**(in_addr **)host->h_addr_list);
                std::cout << "Resolved host '" << host->h_name << "' = " << iptext << std::endl;
                SOCKADDR_IN target;
                target.sin_family = AF_INET;
                target.sin_port = htons(port);
                target.sin_addr.s_addr = inet_addr(iptext);

                SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (sock == INVALID_SOCKET)
                {
                    std::cerr << "Cannot open socket to host" << std::endl;
                }
                else
                {
                    if (0 != connect(sock, (SOCKADDR *)&target, sizeof(target)))
                    {
                        std::cerr << "Cannot connect to host" << std::endl;
                    }
                    else
                    {
                        send(sock, message, strlen(message), 0);
                        send(sock, "\n", 1, 0);

                        const int BUFFERSIZE = 1024;
                        char *response = new char[BUFFERSIZE];
                        int bytesRead = recv(sock, response, BUFFERSIZE, MSG_WAITALL);

                        if (bytesRead <= 0)
                        {
                            std::cerr << "ERROR: no response received" << std::endl;
                        }
                        else
                        {
                            for (int i = 0; i < bytesRead; ++i)
                            {
                                std::cout.put(response[i]);
                            }
                            std::cout << std::endl;
                        }
                    }
                    closesocket(sock);
                    sock = INVALID_SOCKET;
                }

                std::cerr << std::endl;
            }
        }
        else
        {
            std::cerr << "ERROR: Wrong version of WinSock." << std::endl;
        }
        WSACleanup();
    }
    else
    {
        std::cerr << "ERROR: Could not initialize WinSock." << std::endl;
    }
    return 1;
}
