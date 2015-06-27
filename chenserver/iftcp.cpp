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
    , mode(MODE_LINE)
    , httpMinorVersion('\0')
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
    using namespace std;

    line.clear();
    keepRunning = true;
    mode = MODE_LINE;

    CloseSocket(clientSocket);

    if (ready)
    {
        // Wait for an inbound connection.
        clientSocket = accept(hostSocket, nullptr, nullptr);
        if (clientSocket != INVALID_SOCKET)
        {
            const int BUFFERLENGTH = 256;
            char buffer[BUFFERLENGTH];
            static const string HTTP_FIRST_LINE_PREFIX = "GET /";
            static const string HTTP_FIRST_LINE_SUFFIX_0 = " HTTP/1.0\r";
            static const string HTTP_FIRST_LINE_SUFFIX_1 = " HTTP/1.1\r";
            static const char HTTP_HEADER_END[] = "\r\n\r\n";
            assert(HTTP_FIRST_LINE_SUFFIX_0.length() == HTTP_FIRST_LINE_SUFFIX_1.length());
            int headerEndIndex = 0;

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
                    if (mode == MODE_LINE)
                    {
                        if (buffer[i] == '\n')
                        {
                            // We just hit a newline on the first line we read.
                            // If the line looks like the beginning of an HTTP header,
                            // then extract the URI part and decode it as the command.
                            // Example of such a first line:  "GET /status HTTP/1.1\r\n".
                            bool http10 = EndsWith(line, HTTP_FIRST_LINE_SUFFIX_0);
                            bool http11 = EndsWith(line, HTTP_FIRST_LINE_SUFFIX_1);

                            if (StartsWith(line, HTTP_FIRST_LINE_PREFIX) && (http10 || http11))
                            {
                                // Decode the URI from the line and keep it.
                                int offset = HTTP_FIRST_LINE_PREFIX.length();
                                int extract = line.length() - (HTTP_FIRST_LINE_PREFIX.length() + HTTP_FIRST_LINE_SUFFIX_0.length());
                                line = UrlDecode(line.substr(offset, extract));
                                mode = MODE_HTTP;       // remember to send an HTTP response back to the client in WriteLine()
                                httpMinorVersion = http10 ? '0' : '1';      // remember which minor version to use in response
                                // continue reading until we hit end of request header
                            }
                            else
                            {
                                return true;
                            }
                        }

                        line.push_back(buffer[i]);
                    }
                    else if (mode == MODE_HTTP)
                    {
                        // in http mode, keep eating data until we see "\r\n\r\n".
                        if (buffer[i] == HTTP_HEADER_END[headerEndIndex])
                        {
                            if (++headerEndIndex == 4)
                            {
                                return true;
                            }
                        }
                        else
                        {
                            headerEndIndex = 0;
                        }
                    }
                    else
                    {
                        assert(false);  // unknown mode
                        goto read_failure;
                    }
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

read_failure:
    CloseSocket(clientSocket);
    line.clear();
    return false;
}


std::string ChessCommandInterface_tcp::UrlDecode(const std::string& urltext)
{
    using namespace std;

    string decoded;
    int escape = 0;
    int data = 0;

    for (char c : urltext)
    {
        if (escape == 0)
        {
            switch (c)
            {
            case '+':
                decoded.push_back(' ');
                break;

            case '%':
                escape = 2;
                break;

            default:
                if (c <= ' ')
                {
                    return "";  // something is really wrong with the URL
                }
                decoded.push_back(c);
                break;
            }
        }
        else
        {
            c = tolower(c);
            if (c >= '0' && c <= '9')
            {
                data = (data << 4) | (c - '0');
            }
            else if (c >= 'a' && c <= 'f')
            {
                data = (data << 4) | (c - 'a' + 10);
            }
            else
            {
                return "";      // bad input - give up
            }
            if (--escape == 0)
            {
                decoded.push_back(static_cast<char>(data));
            }
        }
    }

    if (escape != 0)
    {
        return "";      // unterminated escape - give up
    }

    return decoded;
}


inline void SendString(SOCKET clientSocket, const std::string& text)
{
    send(clientSocket, text.c_str(), text.length(), 0);
}


inline std::string HttpResponseDate()
{
    // Date: Tue, 15 Nov 1994 08:12:31 GMT\r\n

    // http://stackoverflow.com/questions/2726975/how-can-i-generate-an-rfc1123-date-string-from-c-code-win32

    static const char *DAY_NAMES[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static const char *MONTH_NAMES[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    const int RFC1123_TIME_LEN = 29;
    time_t t;
    struct tm tm;
    char buf[RFC1123_TIME_LEN + 1];

    time(&t);
    gmtime_s(&tm, &t);

    strftime(buf, sizeof(buf), "---, %d --- %Y %H:%M:%S GMT", &tm);
    memcpy(buf, DAY_NAMES[tm.tm_wday], 3);
    memcpy(buf + 8, MONTH_NAMES[tm.tm_mon], 3);

    std::string text = "Date: ";
    text += buf;
    text += "\r\n";

    return text;
}


void ChessCommandInterface_tcp::WriteLine(const std::string& line)
{
    using namespace std;

    if (clientSocket != INVALID_SOCKET)
    {
        string response;

        switch (mode)
        {
        case MODE_HTTP:
            // Send a valid HTTP header back to the client.

            // Start with the status line.
            response = "HTTP/1.";
            response.push_back(httpMinorVersion);
            response += " 200 OK\r\n";

            // Date line
            response += HttpResponseDate();

            // Content type line
            response += "Content-Type: text/plain\r\n";

            // Content length line : include "\r\n" for 2 bytes after the line
            response += "Content-Length: " + to_string(2 + line.length()) + "\r\n";

            // Blank line terminates the header
            response += "\r\n";

            response += line + "\r\n";

            SendString(clientSocket, response);
            break;

        case MODE_LINE:
            SendString(clientSocket, line);
            SendString(clientSocket, "\n");
            break;

        default:
            assert(false);      // unknown mode
            break;
        }

        // Close the client socket because this transaction (ReadLine/WriteLine pair) is now complete.
        CloseSocket(clientSocket);
    }
    else
    {
        std::cerr << "WriteLine ERROR: client socket was not initialized" << std::endl;
    }
}
