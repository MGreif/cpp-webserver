
#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>

using namespace std;
#define REQUEST_CHUNK_SIZE 256
#define REQUEST_MAX_CHUNKS 10
#define REQUEST_HEADER_KEY_LENGTH 128
#define REQUEST_HEADER_VALUE_LENGTH 512
#define RESPONSE_BUFFER_SIZE 1024

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

class Header {
    char name[REQUEST_HEADER_KEY_LENGTH];
    char value[REQUEST_HEADER_VALUE_LENGTH];
public:
    Header(char * n, char * v) {
        strncpy(this->name, n, sizeof(this->name));
        strncpy(this->value, v, sizeof(this->value));
    }
    void toString(char * out, int size) {
        char concatenated[REQUEST_HEADER_KEY_LENGTH + REQUEST_HEADER_VALUE_LENGTH + 2] = {'\0'};
        int nameLength = strlen(name);
        strncpy(concatenated, name, nameLength);
        strncpy(concatenated+nameLength, ": ", 2);
        strncpy(concatenated+nameLength+2, value, strlen(value));
        strncpy(out, concatenated, size);
    }
};

class HeaderMap {

public:
    char* headers[50] = { nullptr };
    int headerCount = 0;

    void addHeader(Header * h) {
        int lastHeaderIndex = this->headerCount;
        this->headers[lastHeaderIndex] = (char*)h;
        headerCount++;
    }

    void toString(char * out, int * outLength, int size) {
        int startOffset = 0;
        int headerMapLength = headerCount * (REQUEST_HEADER_KEY_LENGTH + REQUEST_HEADER_VALUE_LENGTH + 2 + 1); // Headercount * (stringified header length + \n )
        char * tempBuffer = new char[headerMapLength]; 
        for (int i =0; i < headerCount; i++) {
            Header * header = (Header *) headers[i];
            char headerBuffer[REQUEST_HEADER_KEY_LENGTH + REQUEST_HEADER_VALUE_LENGTH + 2] = { '\0' };
            header->toString(headerBuffer, REQUEST_HEADER_KEY_LENGTH + REQUEST_HEADER_VALUE_LENGTH + 2);
            int currentHeaderLength = strlen(headerBuffer);
            strncpy(tempBuffer+startOffset, headerBuffer, currentHeaderLength);
            strcpy(tempBuffer+startOffset+currentHeaderLength, "\n");
            startOffset += currentHeaderLength;
            startOffset += 1;
        }


        *outLength = startOffset;
        strncpy(out, tempBuffer, size);
    }

    ~HeaderMap() {
        this->destroy();
    }

    void destroy() {
        cout << "Destroying HeaderMap" << endl;
        for (int i =0; i< this->headerCount;i++) {
            cout << "Destroying Header " << i << endl;
            delete this->headers[i];
        }
    }
};

class Request {
private:
    char * raw = nullptr;
    int rawLength = 0;
    char method[16] = {0}; 
    char path[512] = {0};
    char protocol[16] = {0};

    char body[512] = {0};

    HeaderMap * headermap = new HeaderMap();

    void buildHeaderMapFromString() {
        if (raw == nullptr) {
            return;
        }

        char requestHeaderPart[rawLength];
        memset(requestHeaderPart, '\0', sizeof(requestHeaderPart)); // Zero out requestHeaderPart
        strncpy(requestHeaderPart, raw, rawLength); // This is currently sufficient, as the raw string only contains the headers. THIS WILL BE CHANGED SOON
        char * dataStartPointer = requestHeaderPart;
        char * ptr1;
        bool skippedMethodHeaderLine = false;
        while((ptr1 = strchr(dataStartPointer, '\n'))!=NULL) {
            if (!skippedMethodHeaderLine) {
                // Skipping first line, as it contains no header, but the METHOD, PATH and PROTOCOL
                skippedMethodHeaderLine = true;
                strcpy(dataStartPointer, ptr1+1);
                continue;
            }
            *ptr1 = '\0'; // \n
            *(ptr1-1) = '\0'; // \r
            char currentHeader[REQUEST_HEADER_KEY_LENGTH + REQUEST_HEADER_VALUE_LENGTH + 3] = { '\0' };
            strncpy(currentHeader, dataStartPointer, strlen(dataStartPointer));

            char * leftOver = ptr1+1;
            dataStartPointer = leftOver;

            char key[REQUEST_HEADER_KEY_LENGTH];
            char value[REQUEST_HEADER_VALUE_LENGTH];
            char * delimeterPtr = strchr(currentHeader, ':');
            if (delimeterPtr == nullptr) {
                continue;
            }
            *delimeterPtr = '\0';
            strncpy(key, currentHeader, sizeof(key));
            strncpy(value, delimeterPtr+2, sizeof(value)); // +2 because in HTTP there is a space behind the colon (': ')
            Header * h = new Header(key, value);
            this->headermap->addHeader(h);

        }
    }
    void populateMethodPathAndProtocol() {
        
        if (this->raw == nullptr) {
            return;
        }
        char requestHeaderPart[rawLength];
        memset(requestHeaderPart, '\0', sizeof(requestHeaderPart)); // Zero out requestHeaderPart
        strncpy(requestHeaderPart, raw, rawLength); // This is currently sufficient, as the raw string only contains the headers. THIS WILL BE CHANGED SOON


        
        char * firstNewLine = strchr(requestHeaderPart, '\n');
        *firstNewLine = '\0';

        char firstLine[rawLength];
        strncpy(firstLine, requestHeaderPart, sizeof(firstLine));

        // Parsing method
        char * afterMethod = strchr(firstLine, '\x20');
        *afterMethod = '\0';
        strncpy(this->method, firstLine, sizeof(firstLine));
  
        // Parsing path
        char * afterPath = strchr(afterMethod + 1, '\x20');
        *afterPath = '\0';
        strncpy(this->path, afterMethod + 1, sizeof(afterMethod));
  
        // Parsing Protocol
        strncpy(this->protocol, afterPath+1, sizeof(afterPath));


    }
public:
    long id;
    Request(long id) {
        this->id = id;
    }
    void setRawRequest(char * raw) {
        this->raw = raw;
        this->rawLength = strlen(raw);
    }
    static void buildFromString(Request * out, char * rawRequest) {
        out->setRawRequest(rawRequest);
        out->parseRequest();
    };
    int parseRequest() {
        if (this->raw == nullptr) {
            return -1;
        }
        buildHeaderMapFromString();
        populateMethodPathAndProtocol();
        return 0;
    }
    char* getPath() {
        return this->path;
    }
    char* getBody() {
        return this->body;
    }
    char* getMethod() {
        return this->method;
    }
    char* getProtocol() {
        return this->protocol;
    }
    HeaderMap* getHeaders() {
        return this->headermap;
    }
    ~Request() {
        cout << "Destroying request" << endl;
    }
};

class Route {

};

// We basically need to check for the end of the HTTP request via the content-length or the transfer-encoding

class RequestReceiver {
private:
    static const int chunkSize = REQUEST_CHUNK_SIZE;
    static const int maxChunks = REQUEST_MAX_CHUNKS;
    long requestEndOffset = 0;
    char* chunkPointer[maxChunks] = {nullptr};
    int chunkCounter = 0;
    char * getNextChunkPointer() {
        if (this->chunkCounter == maxChunks - 1) {
            cout << this->id << ": Max chunks reached ..." << endl;
            return nullptr;
        }
        char * newChunk = new char[chunkSize];
        this->chunkPointer[chunkCounter] = newChunk;
        printf("%d: Allocated chunk %p at counter %d \n", this->id, newChunk, chunkCounter);
        chunkCounter++;
        return newChunk;
    }


    // Just putting current chunks together
    char * getCurrentData() {
        char * allocatedPointer = new char[maxChunks * chunkSize]; // TODO maybe just allocate chunkCounter * chunkSize of chunk is small
        for (int i = 0; i < chunkCounter; i++) {
            strcpy(allocatedPointer + i * chunkSize, chunkPointer[i]);
        }
        return allocatedPointer;
    }
    // Checks if the current chunk contains end of the request. Returns length of request
    // This is still WIP and currently only parses GET Requests
    long getRequestEnd() {

        char requestEndPattern[] = "\r\n\r\n";
        char * currentData = getCurrentData();
        
        char * indexOfStringStart = strstr(currentData, requestEndPattern);
        if (indexOfStringStart == NULL) {
            return 0;
        }
        int len = strlen(requestEndPattern);

        char * indexOfStringEnd = indexOfStringStart + len;

        long lengthOfRequest = indexOfStringEnd - currentData;

        delete[] currentData;
        return lengthOfRequest;
    }
public:
    static const int getMaxChunks() {
        return maxChunks;
    }
    static const int getChunkSize() {
        return chunkSize;
    }
    long id;
    char * fullRequestPointer = nullptr;
    bool requestFinished = false;
    RequestReceiver() {
        this->id = random();
    };

    //Allocate chunks in heap and fill chunks with request data
    static RequestReceiver * receive(RequestReceiver * r, int socket) {
        int ok;

        char tempBuffer[chunkSize];
        while((ok = recv(socket, tempBuffer, chunkSize, 0))) {
            int currentChunk = r->chunkCounter;
            char * nextChunkPointer = r->getNextChunkPointer();
            if (nextChunkPointer == nullptr) {
                cout << r->id << ": Failed to get next chunk pointer. Returning ..." << endl;
                return r;
            } 
            if (ok == -1) {
                cout << r->id << ": Error receiving" << endl;
                printf("%s \n", strerror(errno));
                return r;
            }
            

            memcpy(nextChunkPointer, tempBuffer, chunkSize);
            memset(tempBuffer, '\0', chunkSize);

            printf("%d: Working at chunkindex: %d \n", r->id, currentChunk);

            long requestEndOffset = r->getRequestEnd();
            if (requestEndOffset != 0) {
                printf("%d: End of request detected in chunk %d... \n", r->id, currentChunk);
                long requestEndOffsetInCurrentChunk = requestEndOffset % r->chunkSize;
                printf("%d: requestEndOffset in current chunk: %d \n", r->id, requestEndOffsetInCurrentChunk);
                *(nextChunkPointer + requestEndOffsetInCurrentChunk) = '\0';
                r->requestFinished = true;
                r->requestEndOffset = requestEndOffset;
                return r;
            }
        }
        return r;

    };
    char * getFullRequestString() {
        if (!this->requestFinished) {
            return nullptr;
        }
        if (this->fullRequestPointer != nullptr) {
            return this->fullRequestPointer;
        }
        char * allocatedPointer = getCurrentData();

        this->fullRequestPointer = allocatedPointer;
        return fullRequestPointer;
    };

    // Cleanup all allocated memory
    void destroy() {
        for (int i = 0; i < chunkCounter; i++) {
            cout << this->id << ": Deleting chunk: " << i << endl;
            delete[] chunkPointer[i];
        }
        cout << this->id << ": Deleting fullRequestPointer" << endl;

        delete[] fullRequestPointer;
    }
    ~RequestReceiver() {
        this->destroy();
    }
};

class Router {
    Route routes[10];
    char basePath[32];

    public:
        void handleRoute();
};

void getStatusCodeMapping(char * out, int size, int statusCode) {
    
    switch (statusCode)
    {
    case 200:
        strncpy(out, "OK", size);
        break;
    
    // TODO FILL OTHER STATUS CODES HERE
    default:
        strncpy(out, "OK", size);
        break;
    }
    ;
}

class Response {
private:
    virtual void injectDefaultHeaders(HeaderMap* hm) {};
public:

    char protocol[16] = "HTTP/1.1";
    int statusCode;
    char * responseBufferPtr = nullptr;
    HeaderMap * headers = new HeaderMap();
    char body[512] = { '\0' }; // TODO also use chunking mechanism here, that sends it to the clients socket in chunks
    virtual char * serialize() {
        char * responseBuffer = new char[RESPONSE_BUFFER_SIZE];
        char * stringPointer = responseBuffer;
        
        // PROTOCOL
        strncpy(stringPointer, this->protocol, strlen(this->protocol));
        stringPointer+=strlen(this->protocol);

        strncpy(stringPointer, " ", 1);
        stringPointer+=1;

        // STATUS CODE INT
        char number[10];
        sprintf(number, "%d", this->statusCode);

        strncpy(stringPointer, number, strlen(number));
        stringPointer+=strlen(number);

        strncpy(stringPointer, " ", 1);
        stringPointer+=1;

        // STATUS CODE MAPPING
        char statusCodeMapping[10];
        getStatusCodeMapping(statusCodeMapping, sizeof(statusCodeMapping), this->statusCode);


        strncpy(stringPointer, statusCodeMapping, strlen(statusCodeMapping));
        stringPointer+=strlen(statusCodeMapping);

        strncpy(stringPointer, "\r\n", 2);
        stringPointer+=2;

        // HEADERS
        if (this->headers != nullptr) {
            int headerLength;
            this->headers->toString(stringPointer, &headerLength, RESPONSE_BUFFER_SIZE);

            stringPointer+=headerLength;

        }

        // BODY
        // TODO ADD BODY
        strncpy(stringPointer, "\r\n\r\n", 4);
        stringPointer+=4;

        strncpy(stringPointer, this->body, strlen(this->body));
        stringPointer+=17;

        if (this->responseBufferPtr != NULL) {
            delete[] this->responseBufferPtr;
        }

        this->responseBufferPtr = responseBuffer;

        return responseBuffer;
        
    }
    virtual ~Response() {
        cout << "Destroy Response" << endl;
        delete this->headers;
        delete[] this->responseBufferPtr;
    }
};


class HTMLResponse: public Response {
private:
    void injectDefaultHeaders(HeaderMap* hm) override;
public:
    HTMLResponse() {
        this->injectDefaultHeaders(this->headers);
    }
    ~HTMLResponse() override {
        cout << "Destroying HTMLResponse" <<endl;
    };
};

void HTMLResponse::injectDefaultHeaders(HeaderMap* hm) {
    char ctKey[] = "Content-Type";
    char ctValue[] = "text/html";
    char cKey[] = "Connection";
    char cValue[] = "close";
    Header * contentType = new Header(ctKey, ctValue);
    Header * connection = new Header(cKey, cValue);
    hm->addHeader(contentType);
    hm->addHeader(connection);
}


class ResponseTransmitter {
private:
    int client_socket = 0;
public:
    ResponseTransmitter(int socket_fd) {
        this->client_socket = socket_fd;
    }
    void transmit(Response * r) {
        char * responseBody = r->serialize();
        send(this->client_socket, responseBody, strlen(responseBody), 0);
    }
};


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