#include <stdio.h> /* printf, sprintf */
#include <stdlib.h> /* exit, atoi, malloc, free */
#include <unistd.h> /* read, write, close */
#include <string.h> /* memcpy, memset */

#ifdef _WIN64
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #pragma comment(lib,"ws2_32.lib") //Winsock Library
#else
    #include <sys/socket.h> /* socket, connect */
    #include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
    #include <netdb.h> /* struct hostent, gethostbyname */
#endif

#include "../../inc/common/http.h"

int str_index_of(char search, char* str, int strSize) {
    int index = -1;

    for(int i = 0; i < strSize; i++) {
        if(str[i] == search) {
            index = i;
            break;
        }
    }

    return index;
}

void str_sub_str(char* input, unsigned int inputSize, char* output, unsigned int outputSize, unsigned int startIndex) {
    int start = startIndex;
    // Make sure start is not beyond the input size
    if(start >= inputSize)
        return; // Nothing to copy

    int length = outputSize;
    // Make sure length does not read beyond input size
    if(start + length >= inputSize)
        length = inputSize - start;

    for(int i = 0; i < length; i++) {
        output[i] = input[start + i];
    }

    output[length] = 0;
}

int
HTTP_Get(char* host, char* url, int portno, char* responseBuffer, unsigned int responseBufferSize) {
    char *messageTemplate = "GET http://%s/%s HTTP/1.0\r\n\r\n";
    char message[1024];

    sprintf(message, messageTemplate, host, url);

    struct hostent *server;
    struct sockaddr_in serv_addr;
    int sockfd, bytes, sent, received, total;

    // The internal response buffer should be large enough to
    // hold the response headers and response content. The function caller
    // has already hinted at the content size via the responseBufferSize property.
    // We add an additional 1kb for any headers.
    char response[responseBufferSize + 1024];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) {
        printf("Could not open socket\n");
        return 0;
    }

    server = gethostbyname(host);
    if(server == NULL) {
        printf("No such host\n");
        return 0;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);    
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);

    if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0) {
        printf("ERROR connecting\n");
        return 0;
    }

    /* send the request */
    total = strlen(message);
    sent = 0;
    do {
        bytes = write(sockfd,message+sent,total-sent);
        if (bytes < 0) {
            printf("ERROR writing message to socket\n");
            return 0;
        }
        if (bytes == 0)
            break;
        sent+=bytes;
    } while (sent < total);

    /* receive the response */
    memset(response, 0, sizeof(response));
    total = sizeof(response) - 1;
    received = 0;
    do {
        bytes = read(sockfd, response + received, total - received);
        if (bytes < 0) {
            printf("ERROR reading response from socket\n");
            return 0;
        }
        if (bytes == 0)
            break;
        received+=bytes;
    } while (received < total);

    if (received == total) {
        printf("ERROR storing complete response from socket\n");
        return 0;
    }

    /* close the socket */
    close(sockfd);

    // Start working on reading the response. We must:
    // - Extract the status code
    // - Get the content length
    // - Read the response content

    // Read until we reached the start of the response content.
    // Meanwhile check lines for valid response.
    int index = 0;
    int lineIndex = 0;
    int contentFound = 0;
    int contentLength = -1;
    while(!contentFound && index < sizeof(response)) {
        // Read the current line length
        int lineLength = 0;
        while(index + lineLength < sizeof(response)) {
            char c = response[index + lineLength];

            lineLength++;

            if(c == '\n')
                break;
        }

        // Read line. The line length is minus the carriage return and new line.
        // However because we must zero-terminate the string we subtract only 1.
        char line[lineLength - 1];
        for(int i = 0; i < sizeof(line) - 1; i++) {
            line[i] = response[index + i];
        }
        line[sizeof(line) - 1] = 0;

        // Move index to the next line
        index += lineLength;

        if(sizeof(line) == 1) {
            contentFound = 1;
        }
        else if(lineIndex == 0) {
            // First line should contain http version info
            if(strcmp(line, "HTTP/1.1 200 OK") != 0) {
                printf("Invalid http version or response\n");
                printf("  found -> %s\n", line);
                return 0;
            }
        }
        else {
            // Read header tag and then decide what to do
            int columnIndex = str_index_of(':', line, sizeof(line));
            // printf("Column index @ %i\n", columnIndex);
            if(columnIndex != -1) {
                // Read the tag
                char header[columnIndex + 1];
                char value[sizeof(line) - columnIndex];

                str_sub_str(line, sizeof(line) - 1, header, sizeof(header) - 1, 0);
                str_sub_str(line, sizeof(line) - 1, value, sizeof(value) - 1, columnIndex + 2);

                if(strcmp(header, "content-length") == 0) {
                    // We found the content length
                    contentLength = strtol(value, (char**)NULL, 10);
                }
            }
        }

        lineIndex++;
    }

    if(!contentFound) {
        printf("ERROR no content found\n");
        return 0;
    }

    // Zero output buffer
    memset(responseBuffer, 0, responseBufferSize);

    if(contentLength == -1) {
        // No content length. Assume responseBufferSize is content length
        contentLength = responseBufferSize;
    }
    memcpy(responseBuffer, &response[index], contentLength);

    return 1;
}
