
#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include "webserver.h"
using namespace std;

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

enum HTTPMethod {
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

void interrupt_event(int i) {
    cout << "Caught signal: " << i << endl; 
    int ok = close(server_socket);
    shutdown(server_socket, SHUT_RDWR);
    if (ok == -1) {
        cout << "Failed to close socket: " << server_socket;
    } else {
        cout << "Closed socket: " << server_socket;
    }

    ok = close(client_socket);
    shutdown(client_socket, SHUT_RDWR);
    if (ok == -1) {
        cout << "Failed to close client socket: " << client_socket;
    } else {
        cout << "Closed client socket: " << client_socket;
    }

    exit(1); 
}


int main(int argc, char** argv) {
    
    int soc_fd = socket(AF_INET, SOCK_STREAM, 0);
    server_socket = soc_fd;
    if (soc_fd == -1) {
        cout << "Failed to create socket" << endl;
        return -1;
    }

    int networkPort = 3002;

    sockaddr_in add;
    add.sin_family = AF_INET;
    add.sin_port = htons(networkPort);
    inet_aton("0.0.0.0", &add.sin_addr);


    int ok = bind(soc_fd, (sockaddr*)&add, sizeof(add));
    if (ok  == -1) {
        cout << "Failed to bind socket" << endl;
        printf("%s \n", strerror(errno));
        return -1;
    }
    cout << "Successfully bound socket" << endl;

    ok = listen(soc_fd, 10);
    if (ok  == -1) {
        cout << "Failed to listen" << endl;
        printf("%s \n", strerror(errno));
        return -1;
    }

    cout << "Successfully listening socket on port: " << networkPort  << endl;


    sockaddr client;
    int socket_fd;
    char socket_buffer[512];
    char input_buffer[256];

    signal(SIGINT, interrupt_event);

    while((socket_fd = accept(soc_fd, &client, (socklen_t*)&client))) {
        cout << "Connection accepted ..." << endl;
        client_socket = socket_fd;

        RequestReceiver * receiver;

        while((receiver = RequestReceiver::receive(new RequestReceiver(), client_socket)) != NULL) {     

            char  * fullRequest = receiver->getFullRequestString();
            if (fullRequest == nullptr) {
                break;
            } else {
            }

            Request * request = new Request(receiver->id);
            Request::buildFromString(request, fullRequest);


            printf("%d: Method: %s \n", request->id, request->getMethod());
            printf("%d: Path: %s \n", request->id, request->getPath());
            printf("%d: Protocol: %s \n", request->id, request->getProtocol());
            HeaderMap headers = *request->getHeaders();
            int headerCount = headers.headerCount;
            printf("%d: Header count: %d \n", request->id, headerCount);

            for (int i = 0; i < headerCount; i ++) {
                char headerTemp[REQUEST_HEADER_KEY_LENGTH + REQUEST_HEADER_VALUE_LENGTH + 2] = { '\0'};
                ((Header *)headers.headers[i])->toString(headerTemp, REQUEST_HEADER_KEY_LENGTH + REQUEST_HEADER_VALUE_LENGTH + 2);
                printf("%d: Header -> %s \n", request->id, headerTemp);
                
            }

            ResponseTransmitter rt = ResponseTransmitter(socket_fd);
            Response * r = new HTMLResponse();

            r->statusCode = 200;
            char hK[] = "TEST";
            char hV[] = "A!@#";
            Header testHeader = Header(hK, hV);
            char body[512] = "<em>NOIIIIIICECEE</em>";
            r->headers->addHeader(&testHeader);
            strncpy(r->body, body, sizeof(body));
            
            
            
            rt.transmit(r);
            delete r;
            close(client_socket);



            // Cleanup
            delete request;
            receiver->destroy();

            break;       
            char * socket_buffer_clone = new char[512];
            char * responseBuffer = new char[1028];
            strcpy(socket_buffer_clone, socket_buffer);
            //cout << "Received: " << socket_buffer << " " << ok << endl;

            char * newLinePtr;
            int line = 0;
            while((newLinePtr = strchr(socket_buffer_clone, '\n'))) {
                *newLinePtr = '\0';
                char currentLine[512];
                strcpy(currentLine, socket_buffer_clone);
                char * restOfLine = newLinePtr + 1;
                strcpy(socket_buffer_clone, restOfLine);
                cout << line << ": " << currentLine << endl;

                if (line == 0) {
                    if (strncmp(currentLine, "GET /A", 6) == 0) {
                        cout << "GET A" << endl;
                        strcpy(responseBuffer, "HTTP/1.1 200 OK\nDate: Sun, 10 Oct 2010 23:26:07 GMT\nServer: Apache/2.2.8 (Ubuntu) mod_ssl/2.2.8 OpenSSL/0.9.8g\nLast-Modified: Sun, 26 Sep 2010 22:04:35 GMT\nETag: \"45b6-834-49130cc1182c0\"\nAccept-Ranges: bytes\nContent-Length: 12\nConnection: close\nContent-Type: text/html\n\n<b>NOICE</b>\n\n");
                    }
                    if (strncmp(currentLine, "GET /B", 6) == 0) {
                        cout << "GET B" << endl;
                        strcpy(responseBuffer, "HTTP/1.1 200 OK\nDate: Sun, 10 Oct 2010 23:26:07 GMT\nServer: Apache/2.2.8 (Ubuntu) mod_ssl/2.2.8 OpenSSL/0.9.8g\nLast-Modified: Sun, 26 Sep 2010 22:04:35 GMT\nETag: \"45b6-834-49130cc1182c0\"\nAccept-Ranges: bytes\nContent-Length: 12\nConnection: close\nContent-Type: text/html\n\n<b>Hallo Belli</b>\n\n");
                    }
                }

                line++;
            }




            send(socket_fd, responseBuffer, strlen(responseBuffer), 0);
            /*
            char * ptr = strchr(socket_buffer,'\n');
            if (ptr != NULL) {
                *ptr = '\0';
            }
            */
            //cout << "Received: " << socket_buffer << " " << ok << endl;
            memset(socket_buffer, '\0', sizeof(socket_buffer));
            break;
            /*
            cout << "Input: ";
            cin >> input_buffer;
            ptr = strchr(input_buffer,'\n');
            if (ptr != NULL) {
                *(ptr+1) = '\0';
            }

            send(socket_fd, input_buffer, strlen(input_buffer), 0);
            */
            
        }
        cout << endl;
        continue;
    }


    return 0;
}