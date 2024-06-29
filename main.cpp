
#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include "webserver.h"

// Overwriting defaults
#ifdef WEBSERVER_DEFAULT_DEFINITIONS
#undef REQUEST_CHUNK_SIZE
#undef REQUEST_MAX_CHUNKS
#undef REQUEST_HEADER_KEY_LENGTH
#undef REQUEST_HEADER_VALUE_LENGTH
#undef RESPONSE_BUFFER_SIZE
#define REQUEST_CHUNK_SIZE 256
#define REQUEST_MAX_CHUNKS 10
#define REQUEST_HEADER_KEY_LENGTH 128
#define REQUEST_HEADER_VALUE_LENGTH 512
#define RESPONSE_BUFFER_SIZE 1024
#endif

using namespace std;

enum HTTPMethod
{
    GET,
    POST,
    PATCH,
    PUT,
    DELETE,
    OPTIONS,
    TRACE
};

int server_socket;
int client_socket;

void interrupt_event(int i)
{
    cout << "Caught signal: " << i << endl;
    int ok = close(server_socket);
    shutdown(server_socket, SHUT_RDWR);
    if (ok == -1)
    {
        cout << "Failed to close socket: " << server_socket;
    }
    else
    {
        cout << "Closed socket: " << server_socket;
    }

    ok = close(client_socket);
    shutdown(client_socket, SHUT_RDWR);
    if (ok == -1)
    {
        cout << "Failed to close client socket: " << client_socket;
    }
    else
    {
        cout << "Closed client socket: " << client_socket;
    }

    exit(1);
}

int main(int argc, char **argv)
{

    int soc_fd = socket(AF_INET, SOCK_STREAM, 0);
    server_socket = soc_fd;
    if (soc_fd == -1)
    {
        cout << "Failed to create socket" << endl;
        return -1;
    }

    if (argc == 1)
    {
        printf("usage: %s <port>", argv[0]);
        exit(1);
    }

    int networkPort;
    int ok = sscanf(argv[1], "%d", &networkPort);
    if (ok != 1)
    {
        printf("usage: %s <port:int>", argv[0]);
    }

    sockaddr_in add;
    add.sin_family = AF_INET;
    add.sin_port = htons(networkPort);
    inet_aton("0.0.0.0", &add.sin_addr);

    ok = bind(soc_fd, (sockaddr *)&add, sizeof(add));
    if (ok == -1)
    {
        cout << "Failed to bind socket" << endl;
        printf("%s \n", strerror(errno));
        return -1;
    }
    cout << "Successfully bound socket" << endl;

    ok = listen(soc_fd, 10);
    if (ok == -1)
    {
        cout << "Failed to listen" << endl;
        printf("%s \n", strerror(errno));
        return -1;
    }

    cout << "Successfully listening socket on port: " << networkPort << endl;

    sockaddr client;
    socklen_t *client_len = (socklen_t *)&client;
    int socket_fd;

    signal(SIGINT, interrupt_event);

    while ((socket_fd = accept(soc_fd, &client, client_len)))
    {
        cout << "Connection accepted ..." << endl;
        client_socket = socket_fd;

        RequestReceiver *receiver;
        // TODO: Inject request pointer into RequestReceiver. Let the receiver fill the request.
        // Also maybe adjust this code, to use only one request receiver
        while ((receiver = RequestReceiver::receive(new RequestReceiver(), client_socket)) != NULL)
        {

            char *fullRequest = receiver->getFullRequestString();
            if (fullRequest == nullptr)
            {
                break;
            }
            else
            {
            }

            Request *request = new Request(receiver->id);
            Request::buildFromString(request, fullRequest);

            printf("%ld: Method: %s \n", request->id, request->getMethod());
            printf("%ld: Path: %s \n", request->id, request->getPath());
            printf("%ld: Protocol: %s \n", request->id, request->getProtocol());
            HeaderMap headers = *request->getHeaders();
            int headerCount = headers.headerCount;
            printf("%ld: Header count: %d \n", request->id, headerCount);

            for (int i = 0; i < headerCount; i++)
            {
                char headerTemp[REQUEST_HEADER_KEY_LENGTH + REQUEST_HEADER_VALUE_LENGTH + 2] = {'\0'};
                ((Header *)headers.headers[i])->toString(headerTemp, REQUEST_HEADER_KEY_LENGTH + REQUEST_HEADER_VALUE_LENGTH + 2);
                printf("%ld: Header -> %s \n", request->id, headerTemp);
            }

            ResponseTransmitter rt = ResponseTransmitter(socket_fd);
            Response *r = new HTMLResponse();

            r->statusCode = 200;
            char hK[] = "TEST";
            char hV[] = "A!@#";
            Header testHeader = Header(hK, hV);
            char body[512] = "<em>It Works ...</em>";
            r->headers->addHeader(&testHeader);
            strncpy(r->body, body, sizeof(body));

            rt.transmit(r);
            close(client_socket);

            // Cleanup
            delete request;
            receiver->destroy();
        }
    }
    return 0;
}
