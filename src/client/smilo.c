#include "stdio.h"
#include "string.h"
#include "../../inc/common/http.h"
#include "../../inc/client/smilo.h"

void CL_Smilo_Connected(char* id, char* contractAddress) {
    printf("Client connected! Notifying Smilo Client Agent...\n");

    // Format url to contain query parameter
    char url[1024];
    char* urlTemplate = "v1/client/participate?uid=%s&contractAddress=%s";
    sprintf(url, urlTemplate, id, contractAddress);

    char response[4096];
    if(HTTP_Get("127.0.0.1", url, 8090, response, sizeof(response))) {
        printf("  Agent response: %s\n", response);
    }
    else {
        printf("Failed to do HTTP call...\n");
    }
}

int CL_Smilo_BetConfirmed() {
    printf("Checking if bet is confirmed...\n");

    // Notify Smilo server agent
    char response[4096];
    if(HTTP_Get("127.0.0.1", "v1/client/betconfirmed", 8090, response, sizeof(response))) {
        printf("  Agent response: %s\n", response);
        if(!strcmp(response, "true"))
            return 1;
        else
            return 0;
    }
    else {
        printf("Failed to do HTTP call...\n");
        return 0;
    }
}
