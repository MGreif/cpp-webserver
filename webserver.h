
#ifndef WEBSERVER
#define WEBSERVER
#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>

#define WEBSERVER_DEFAULT_DEFINITIONS
#define REQUEST_CHUNK_SIZE 256
#define REQUEST_MAX_CHUNKS 10
#define REQUEST_HEADER_KEY_LENGTH 128
#define REQUEST_HEADER_VALUE_LENGTH 512
#define RESPONSE_BUFFER_SIZE 1024

using namespace std;

class Header
{
    char name[REQUEST_HEADER_KEY_LENGTH];
    char value[REQUEST_HEADER_VALUE_LENGTH];

public:
    Header(char *n, char *v)
    {
        strncpy(this->name, n, sizeof(this->name));
        strncpy(this->value, v, sizeof(this->value));
    }
    void toString(char *out, int size);
};

class HeaderMap
{

public:
    char *headers[50] = {nullptr};
    int headerCount = 0;

    void addHeader(Header *h);
    void toString(char *out, int *outLength, int size);

    ~HeaderMap()
    {
        this->destroy();
    }

    void destroy();
};

class Request
{
private:
    char *raw = nullptr;
    int rawLength = 0;
    char method[16] = {0};
    char path[512] = {0};
    char protocol[16] = {0};
    char body[512] = {0};

    HeaderMap *headermap = new HeaderMap();

    void buildHeaderMapFromString();
    void populateMethodPathAndProtocol();

public:
    long id;
    Request(long id)
    {
        this->id = id;
    }
    void setRawRequest(char *raw)
    {
        this->raw = raw;
        this->rawLength = strlen(raw);
    }
    static void buildFromString(Request *out, char *rawRequest);
    int parseRequest();
    char *getPath()
    {
        return this->path;
    }
    char *getBody()
    {
        return this->body;
    }
    char *getMethod()
    {
        return this->method;
    }
    char *getProtocol()
    {
        return this->protocol;
    }
    HeaderMap *getHeaders()
    {
        return this->headermap;
    }
    ~Request()
    {
        cout << "Destroying request" << endl;
    }
};

class Route
{
};

// We basically need to check for the end of the HTTP request via the content-length or the transfer-encoding

class RequestReceiver
{
private:
    static const int chunkSize = REQUEST_CHUNK_SIZE;
    static const int maxChunks = REQUEST_MAX_CHUNKS;
    long requestEndOffset = 0;
    char *chunkPointer[maxChunks] = {nullptr};
    int chunkCounter = 0;
    char *getNextChunkPointer();

    // Just putting current chunks together
    char *getCurrentData();
    // Checks if the current chunk contains end of the request. Returns length of request
    // This is still WIP and currently only parses GET Requests
    long getRequestEnd();

public:
    static const int getMaxChunks()
    {
        return maxChunks;
    }
    static const int getChunkSize()
    {
        return chunkSize;
    }
    long id;
    int *client_socket = nullptr;
    char *fullRequestPointer = nullptr;
    bool requestFinished = false;
    RequestReceiver()
    {
        this->id = random();
    };

    // Allocate chunks in heap and fill chunks with request data
    int receive(int *socket);
    char *getFullRequestString();

    // Cleanup all allocated memory
    void destroy();
    void close_connection();
    ~RequestReceiver()
    {
        this->destroy();
    }
};

class Router
{
    Route routes[10];
    char basePath[32];

public:
    void handleRoute();
};

class Response
{
private:
    virtual void injectDefaultHeaders(HeaderMap *hm) {};

public:
    char protocol[16] = "HTTP/1.1";
    int statusCode;
    char *responseBufferPtr = nullptr;
    HeaderMap *headers = new HeaderMap();
    char body[512] = {'\0'}; // TODO also use chunking mechanism here, that sends it to the clients socket in chunks
    virtual char *serialize();
    virtual ~Response();
};

class HTMLResponse : public Response
{
private:
    void injectDefaultHeaders(HeaderMap *hm) override;

public:
    HTMLResponse()
    {
        this->injectDefaultHeaders(this->headers);
    }
    ~HTMLResponse() override
    {
        cout << "Destroying HTMLResponse" << endl;
    };
};

class ResponseTransmitter
{
private:
    int client_socket = 0;

public:
    ResponseTransmitter(int socket_fd);
    void transmit(Response *r);
};

#endif
