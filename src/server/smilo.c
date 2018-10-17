#include "stdio.h"
#include "string.h"
#include "../../inc/common/http.h"
#include "../../inc/server/smilo.h"

char confirmedPlayerUids[128];
int playeruidsIndex;

void
SV_Smilo_StartMatch() {
    printf("Match start! Notifying Smilo Server Agent...\n");

    // Notify Smilo server agent
    char response[4096];
    if(HTTP_Get("127.0.0.1", "v1/server/startround", 8080, response, sizeof(response))) {
        printf("  Agent response: %s\n", response);
    }
    else {
        printf("Failed to do HTTP call...\n");
    }
}

void
SV_Smilo_EndMatch(char* score_list) {
    printf("Match end! Notifying Smilo Server Agent...\n");

    // Format url to contain query parameter
    char url[1024];
    char* urlTemplate = "v1/server/endround?gameResults=%s";
    sprintf(url, urlTemplate, score_list);

    // Notify Smilo server agent
    char response[4096];
    if(HTTP_Get("127.0.0.1", url, 8080, response, sizeof(response))) {
        printf("  Agent response: %s\n", response);
    }
    else {
        printf("Failed to do HTTP call...\n");
    }
}

int SV_Smilo_BetConfirmed(int uniqueId) {
    printf("Checking confirmed status for %i...\n", uniqueId);

    // Format url to contain query parameter
    char url[1024];
    char* urlTemplate = "v1/server/isvalidparticipant?uid=%i";
    sprintf(url, urlTemplate, uniqueId);

    // Notify Smilo server agent
    char response[4096];
    if(HTTP_Get("127.0.0.1", url, 8080, response, sizeof(response))) {
        printf("  Agent response: %s\n", response);
        if (!strcmp(response, "true")) {
            printf("  (SV) BET CONFIRMED: 1! \n");
            confirmedPlayerUids[playeruidsIndex] = uniqueId;
            playeruidsIndex++;
            return 1;
        } else {
            printf("  (SV) BET CONFIRMED: 0! \n");
            return 0;
        }
    }
    else {
        printf("Failed to do HTTP call...\n");
        return 0;
    }
}

int SV_Smilo_GetContractAddress(char* buffer, int bufferSize) {
    printf("Requesting contract address...\n");

    // Notify Smilo server agent
    char response[4096];
    if(HTTP_Get("127.0.0.1", "v1/server/contractaddress", 8080, response, sizeof(response))) {
        printf("  Agent response: %s\n", response);
        
        // Copy response in buffer
        strncpy(buffer, response, bufferSize);

        return 1;
    }
    else {
        printf("Failed to do HTTP call...\n");
        return 0;
    }
}
