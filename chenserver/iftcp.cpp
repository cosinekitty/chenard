/*
    iftcp.cpp  -  Don Cross  -  http://cosinekitty.com
*/

#include "chenserver.h"

ChessCommandInterface_tcp::ChessCommandInterface_tcp(int _port)
    : port(_port)
    , initialized(false)
    , ready(false)
    , hostSocket(INVALID_SOCKET)
    , clientSocket(INVALID_SOCKET)
{
    WSADATA data;
    const WORD versionRequested = MAKEWORD(2, 2);
    int result = WSAStartup(versionRequested, &data);
    if (result == 0)
    {
        initialized = true;     // tells us we need to call WSACleanup() in destructor

        if (data.wVersion == versionRequested)
        {
            // Open the socket.
            SOCKADDR_IN addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = htonl(INADDR_ANY);

            // Wait for an inbound TCP connection.
            hostSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (hostSocket != INVALID_SOCKET)
            {
                // Bind socket to the port number.
                if (0 == bind(hostSocket, (LPSOCKADDR)&addr, sizeof(addr)))
                {
                    if (0 == listen(hostSocket, 2))
                    {
                        ready = true;       // indicates that we are ready to wait for inbound connections
                    }
                    else
                    {
                        std::cerr << "ERROR: Could not set host socket to listen mode." << std::endl;
                    }
                }
                else
                {
                    std::cerr << "ERROR: Could not bind host socket to port." << std::endl;
                }
            }
            else
            {
                std::cerr << "ERROR: Could not open host socket." << std::endl;
            }
        }
        else
        {
            std::cerr << "ERROR: Invalid WinSock version" << std::endl;
        }
    }
    else
    {
        std::cerr << "ERROR: WSAStartup() returned " << result << std::endl;
    }
}


ChessCommandInterface_tcp::~ChessCommandInterface_tcp()
{
    ready = false;
    CloseSocket(clientSocket);
    CloseSocket(hostSocket);

    if (initialized)
    {
        WSACleanup();
        initialized = false;
    }
}


bool ChessCommandInterface_tcp::ReadLine(std::string& line, bool& keepRunning)
{
    line.clear();
    keepRunning = true;

    CloseSocket(clientSocket);

    if (ready)
    {
        // Wait for an inbound connection.
        clientSocket = accept(hostSocket, nullptr, nullptr);
        if (clientSocket != INVALID_SOCKET)
        {
            const int BUFFERLENGTH = 256;
            char buffer[BUFFERLENGTH];

            while (true)
            {
                int bytes = recv(clientSocket, buffer, sizeof(buffer), 0);
                if (bytes <= 0)
                {
                    std::cerr << "ERROR reading from client." << std::endl;
                    break;
                }

                for (int i = 0; i < bytes; ++i)
                {
                    if (buffer[i] == '\n')
                    {
                        return true;
                    }
                    line.push_back(buffer[i]);
                }
            }
        }
        else
        {
            std::cerr << "ReadLine ERROR: Could not accept client socket connection." << std::endl;
        }
    }
    else
    {
        std::cerr << "ReadLine ERROR: client socket was not initialized" << std::endl;
    }

    line.clear();
    return false;
}


void ChessCommandInterface_tcp::WriteLine(const std::string& line)
{
    if (clientSocket != INVALID_SOCKET)
    {
        // Write the response back to the client.
        send(clientSocket, line.c_str(), line.length(), 0);
        
        // Append a newline character so the caller doesn't have to.
        send(clientSocket, "\n", 1, 0);

        // Close the client socket because this transaction (ReadLine/WriteLine pair) is now complete.
        CloseSocket(clientSocket);
    }
    else
    {
        std::cerr << "WriteLine ERROR: client socket was not initialized" << std::endl;
    }
}
