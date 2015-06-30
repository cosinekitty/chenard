/*
    main.cpp  -  Sample client for ChenServer, by Don Cross.
    
    https://github.com/cosinekitty/chenard/wiki/ChenServer
*/

#define CHENARD_LINUX   (defined(__linux__) || defined(__APPLE__))    

#include <stdlib.h>
#include <string>
#include <iostream>
#if CHENARD_LINUX
    #include <stdlib.h>
    #include <unistd.h>
    #include <cstring>
    #include <sys/types.h> 
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    
    // Hacks to help WinSock code build on Linux.
    typedef int SOCKET;
    typedef sockaddr_in SOCKADDR_IN;
    typedef sockaddr *LPSOCKADDR;
    const int INVALID_SOCKET = -1;
    
    #define closesocket close
#else
    #ifdef _MSC_VER     // Windows?
        #define _WINSOCK_DEPRECATED_NO_WARNINGS
        #include <WinSock2.h>
        #include <WS2tcpip.h>
    #else
        #error We do not know how to do socket programming on this platform.
    #endif
#endif

bool SendCommand(const std::string& server, int port, const std::string& command, std::string& response);

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

#ifdef _MSC_VER
    WSADATA data;
    const WORD versionRequested = MAKEWORD(2, 2);
    int result = WSAStartup(versionRequested, &data);
    if (result == 0)
    {
        if (data.wVersion == versionRequested)
        {
#endif        
            std::string response;
            if (SendCommand(server, port, message, response))
            {
                std::cout << response << std::endl;
            }
#ifdef _MSC_VER
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
#endif        
    return 1;
}


bool SendCommand(const std::string& server, int port, const std::string& command, std::string& response)
{
    bool success = false;
    response.clear();

    hostent *host = gethostbyname(server.c_str());
    if (host == nullptr)
    {
        std::cerr << "ERROR: Cannot resolve host '" << server << "'" << std::endl;
    }
    else
    {
        const std::string iptext = inet_ntoa(**(in_addr **)host->h_addr_list);
        std::cout << "Resolved host '" << host->h_name << "' = " << iptext << std::endl;
        SOCKADDR_IN target;
        target.sin_family = AF_INET;
        target.sin_port = htons(port);
        target.sin_addr.s_addr = inet_addr(iptext.c_str());

        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET)
        {
            std::cerr << "Cannot open socket to host" << std::endl;
        }
        else
        {
            if (0 != connect(sock, (LPSOCKADDR)&target, sizeof(target)))
            {
                std::cerr << "Cannot connect to host" << std::endl;
            }
            else
            {
                send(sock, command.c_str(), command.length(), 0);
                send(sock, "\n", 1, 0);

                while (true)    // keep looping and reading blocks of memory until we find "\n" or hit an error.
                {
                    const int BUFFERSIZE = 128;
                    char buffer[BUFFERSIZE];
                    int bytesRead = recv(sock, buffer, BUFFERSIZE, 0);
                    if (bytesRead < 0)
                    {
                        std::cerr << "ERROR: Problem reading response from socket." << std::endl;
                        break;
                    }

                    if (bytesRead == 0)
                    {
                        std::cerr << "ERROR: Incomplete response." << std::endl;
                        break;
                    }

                    for (int i = 0; i < bytesRead; ++i)
                    { 
                        if (buffer[i] == '\n')
                        {
                            // We found the end of the message.
                            success = true;
                            goto end_of_message;    // break out of both loops
                        }
                        response.push_back(buffer[i]);
                    }
                }
            }
end_of_message:
            closesocket(sock);
        }
    }
    if (!success)
    {
        response.clear();
    }
    return success;
}
