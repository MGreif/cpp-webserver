#include <cstring>
#include <iostream>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include "webserver.h"

#define WEBSERVER_DEFAULT_DEFINITIONS
#define REQUEST_CHUNK_SIZE 256
#define REQUEST_MAX_CHUNKS 10
#define REQUEST_HEADER_KEY_LENGTH 128
#define REQUEST_HEADER_VALUE_LENGTH 512
#define RESPONSE_BUFFER_SIZE 1024

using namespace std;

// Header
void Header::toString(char *out, int size)
{
    char concatenated[REQUEST_HEADER_KEY_LENGTH + REQUEST_HEADER_VALUE_LENGTH + 2] = {'\0'};
    int nameLength = strlen(name);
    strncpy(concatenated, name, nameLength);
    strncpy(concatenated + nameLength, ": ", 2);
    strncpy(concatenated + nameLength + 2, value, strlen(value));
    strncpy(out, concatenated, size);
}

// HeaderMap
void HeaderMap::addHeader(Header *h)
{
    int lastHeaderIndex = this->headerCount;
    this->headers[lastHeaderIndex] = (char *)h;
    headerCount++;
}

void HeaderMap::toString(char *out, int *outLength, int size)
{
    int startOffset = 0;
    int headerMapLength = headerCount * (REQUEST_HEADER_KEY_LENGTH + REQUEST_HEADER_VALUE_LENGTH + 2 + 1); // Headercount * (stringified header length + \n )
    char *tempBuffer = new char[headerMapLength];
    for (int i = 0; i < headerCount; i++)
    {
        Header *header = (Header *)headers[i];
        char headerBuffer[REQUEST_HEADER_KEY_LENGTH + REQUEST_HEADER_VALUE_LENGTH + 2] = {'\0'};
        header->toString(headerBuffer, REQUEST_HEADER_KEY_LENGTH + REQUEST_HEADER_VALUE_LENGTH + 2);
        int currentHeaderLength = strlen(headerBuffer);
        strncpy(tempBuffer + startOffset, headerBuffer, currentHeaderLength);
        strcpy(tempBuffer + startOffset + currentHeaderLength, "\n");
        startOffset += currentHeaderLength;
        startOffset += 1;
    }

    *outLength = startOffset;
    strncpy(out, tempBuffer, size);
}

void HeaderMap::destroy()
{
    cout << "Destroying HeaderMap" << endl;
    cout << "Destroying headers: " << this->headerCount << endl;
    for (int i = 0; i < this->headerCount; i++)
    {
        cout << "Destroying Header " << i << endl;
        delete this->headers[i];
    }
    cout << "Destroyed headers" << endl;
}

// Request

void Request::buildHeaderMapFromString()
{
    if (raw == nullptr)
    {
        return;
    }

    char requestHeaderPart[rawLength];
    memset(requestHeaderPart, '\0', sizeof(requestHeaderPart)); // Zero out requestHeaderPart
    strncpy(requestHeaderPart, raw, rawLength);                 // This is currently sufficient, as the raw string only contains the headers. THIS WILL BE CHANGED SOON
    char *dataStartPointer = requestHeaderPart;
    char *ptr1;
    bool skippedMethodHeaderLine = false;
    while ((ptr1 = strchr(dataStartPointer, '\n')) != NULL)
    {
        if (!skippedMethodHeaderLine)
        {
            // Skipping first line, as it contains no header, but the METHOD, PATH and PROTOCOL
            skippedMethodHeaderLine = true;
            strcpy(dataStartPointer, ptr1 + 1);
            continue;
        }
        *ptr1 = '\0';       // \n
        *(ptr1 - 1) = '\0'; // \r
        char currentHeader[REQUEST_HEADER_KEY_LENGTH + REQUEST_HEADER_VALUE_LENGTH + 3] = {'\0'};
        strncpy(currentHeader, dataStartPointer, strlen(dataStartPointer));

        char *leftOver = ptr1 + 1;
        dataStartPointer = leftOver;

        char key[REQUEST_HEADER_KEY_LENGTH];
        char value[REQUEST_HEADER_VALUE_LENGTH];
        char *delimeterPtr = strchr(currentHeader, ':');
        if (delimeterPtr == nullptr)
        {
            continue;
        }
        *delimeterPtr = '\0';
        strncpy(key, currentHeader, sizeof(key));
        strncpy(value, delimeterPtr + 2, sizeof(value)); // +2 because in HTTP there is a space behind the colon (': ')
        Header *h = new Header(key, value);
        this->headermap->addHeader(h);
    }
}
void Request::populateMethodPathAndProtocol()
{

    if (this->raw == nullptr)
    {
        return;
    }
    char requestHeaderPart[rawLength];
    memset(requestHeaderPart, '\0', sizeof(requestHeaderPart)); // Zero out requestHeaderPart
    strncpy(requestHeaderPart, raw, rawLength);                 // This is currently sufficient, as the raw string only contains the headers. THIS WILL BE CHANGED SOON

    char *firstNewLine = strchr(requestHeaderPart, '\n');
    *firstNewLine = '\0';

    char firstLine[rawLength];
    strncpy(firstLine, requestHeaderPart, sizeof(firstLine));

    // Parsing method
    char *afterMethod = strchr(firstLine, '\x20');
    *afterMethod = '\0';
    strncpy(this->method, firstLine, sizeof(this->method));

    // Parsing path
    char *afterPath = strchr(afterMethod + 1, '\x20');
    *afterPath = '\0';
    strncpy(this->path, afterMethod + 1, sizeof(afterMethod));

    // Parsing Protocol
    strncpy(this->protocol, afterPath + 1, sizeof(afterPath));
}

void Request::buildFromString(Request *out, char *rawRequest)
{
    out->setRawRequest(rawRequest);
    out->parseRequest();
};
int Request::parseRequest()
{
    if (this->raw == nullptr)
    {
        return -1;
    }
    buildHeaderMapFromString();
    populateMethodPathAndProtocol();
    return 0;
}

// RequestReceiver
// We basically need to check for the end of the HTTP request via the content-length or the transfer-encoding

char *RequestReceiver::getNextChunkPointer()
{
    if (this->chunkCounter == maxChunks - 1)
    {
        cout << this->id << ": Max chunks reached ..." << endl;
        return nullptr;
    }
    char *newChunk = new char[chunkSize];
    this->chunkPointer[chunkCounter] = newChunk;
    printf("%ld: Allocated chunk %p at counter %d \n", this->id, newChunk, chunkCounter);
    chunkCounter++;
    return newChunk;
}

// Just putting current chunks together
char *RequestReceiver::getCurrentData()
{
    char *allocatedPointer = new char[maxChunks * chunkSize]; // TODO maybe just allocate chunkCounter * chunkSize of chunk is small
    for (int i = 0; i < chunkCounter; i++)
    {
        strcpy(allocatedPointer + i * chunkSize, chunkPointer[i]);
    }
    return allocatedPointer;
}
// Checks if the current chunk contains end of the request. Returns length of request
// This is still WIP and currently only parses GET Requests
long RequestReceiver::getRequestEnd()
{

    char requestEndPattern[] = "\r\n\r\n";
    char *currentData = getCurrentData();

    char *indexOfStringStart = strstr(currentData, requestEndPattern);
    if (indexOfStringStart == NULL)
    {
        return 0;
    }
    int len = strlen(requestEndPattern);

    char *indexOfStringEnd = indexOfStringStart + len;

    long lengthOfRequest = indexOfStringEnd - currentData;

    delete[] currentData;
    return lengthOfRequest;
}

// Allocate chunks in heap and fill chunks with request data
int RequestReceiver::receive(int *socket)
{
    int ok;
    cout << *socket << endl;
    if (socket == nullptr || *socket == 0 || *socket == -1)
    {
        return 0;
    }
    this->client_socket = socket;
    char tempBuffer[chunkSize];
    while ((ok = recv(*socket, tempBuffer, chunkSize, 0)))
    {
        if (ok == -1)
        {
            cout << this->id << ": Error receiving" << endl;
            printf("Error: %s \n", strerror(errno));
            return -1;
        }
        cout << "Read: " << ok << " Bytes" << endl;
        int currentChunk = this->chunkCounter;
        char *nextChunkPointer = this->getNextChunkPointer();
        if (nextChunkPointer == nullptr)
        {
            cout << this->id << ": Failed to get next chunk pointer. Returning ..." << endl;
            return -1;
        }

        memcpy(nextChunkPointer, tempBuffer, chunkSize);
        memset(tempBuffer, '\0', chunkSize);

        printf("%ld: Working at chunkindex: %d \n", this->id, currentChunk);

        long requestEndOffset = this->getRequestEnd();
        if (requestEndOffset != 0)
        {
            printf("%ld: End of request detected in chunk %d... \n", this->id, currentChunk);
            long requestEndOffsetInCurrentChunk = requestEndOffset % this->chunkSize;
            printf("%ld: requestEndOffset in current chunk: %ld \n", this->id, requestEndOffsetInCurrentChunk);
            *(nextChunkPointer + requestEndOffsetInCurrentChunk) = '\0';
            this->requestFinished = true;
            this->requestEndOffset = requestEndOffset;
            return requestEndOffset;
        }
    }
    return 0;
};
char *RequestReceiver::getFullRequestString()
{
    if (!this->requestFinished)
    {
        return nullptr;
    }
    if (this->fullRequestPointer != nullptr)
    {
        return this->fullRequestPointer;
    }
    char *allocatedPointer = getCurrentData();

    this->fullRequestPointer = allocatedPointer;
    return fullRequestPointer;
};

// Cleanup all allocated memory
void RequestReceiver::destroy()
{
    for (int i = 0; i < chunkCounter; i++)
    {
        cout << this->id << ": Deleting chunk: " << i << endl;
        delete[] chunkPointer[i];
    }
    cout << this->id << ": Deleting fullRequestPointer" << endl;

    delete[] fullRequestPointer;
}

void RequestReceiver::close_connection()
{
    if (this->client_socket == nullptr)
    {
        cout << "Client socket not initialized or already closed" << endl;
    }
    close(*this->client_socket);
    cout << "Closing socket connection: " << this->client_socket << endl;
    *this->client_socket = 0;
    this->client_socket = nullptr;
}

void getStatusCodeMapping(char *out, int size, int statusCode)
{

    switch (statusCode)
    {
    case 200:
        strncpy(out, "OK", size);
        break;

    // TODO FILL OTHER STATUS CODES HERE
    default:
        strncpy(out, "OK", size);
        break;
    };
}

// Response
char *Response::serialize()
{
    char *responseBuffer = new char[RESPONSE_BUFFER_SIZE];
    char *stringPointer = responseBuffer;

    // PROTOCOL
    strncpy(stringPointer, this->protocol, strlen(this->protocol));
    stringPointer += strlen(this->protocol);

    strncpy(stringPointer, " ", 1);
    stringPointer += 1;

    // STATUS CODE INT
    char number[10];
    sprintf(number, "%d", this->statusCode);

    strncpy(stringPointer, number, strlen(number));
    stringPointer += strlen(number);

    strncpy(stringPointer, " ", 1);
    stringPointer += 1;

    // STATUS CODE MAPPING
    char statusCodeMapping[10];
    getStatusCodeMapping(statusCodeMapping, sizeof(statusCodeMapping), this->statusCode);

    strncpy(stringPointer, statusCodeMapping, strlen(statusCodeMapping));
    stringPointer += strlen(statusCodeMapping);

    strncpy(stringPointer, "\r\n", 2);
    stringPointer += 2;

    // HEADERS
    if (this->headers != nullptr)
    {
        int headerLength;
        this->headers->toString(stringPointer, &headerLength, RESPONSE_BUFFER_SIZE);

        stringPointer += headerLength;
    }

    // BODY
    // TODO ADD BODY
    strncpy(stringPointer, "\r\n\r\n", 4);
    stringPointer += 4;

    strncpy(stringPointer, this->body, strlen(this->body));
    stringPointer += 17;

    if (this->responseBufferPtr != NULL)
    {
        delete[] this->responseBufferPtr;
    }

    this->responseBufferPtr = responseBuffer;

    return responseBuffer;
}
Response::~Response()
{
    cout << "Destroy Response" << endl;
    delete this->headers;
    delete[] this->responseBufferPtr;
}

// HTMLResponse

void HTMLResponse::injectDefaultHeaders(HeaderMap *hm)
{
    char ctKey[] = "Content-Type";
    char ctValue[] = "text/html";
    char cKey[] = "Connection";
    char cValue[] = "close";
    Header *contentType = new Header(ctKey, ctValue);
    Header *connection = new Header(cKey, cValue);
    hm->addHeader(contentType);
    hm->addHeader(connection);
}

// ResponseTransmitter

ResponseTransmitter::ResponseTransmitter(int socket_fd)
{
    this->client_socket = socket_fd;
}
void ResponseTransmitter::transmit(Response *r)
{
    char *responseBody = r->serialize();
    send(this->client_socket, responseBody, strlen(responseBody), 0);
}
