#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <stdlib.h>
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

int CL_Smilo_GetBalance(char* uid) {
    printf("Get client balance!\n");

    // Format url to contain query parameter
    char url[1024];
    char* urlTemplate = "v1/client/balance?uid=%s";
    sprintf(url, urlTemplate, uid);

    char response[4096];
    if(HTTP_Get("127.0.0.1", url, 8090, response, sizeof(response))) {
        printf("  Agent response: %s\n", response);
        return atoi(response);
    }
    else {
        printf("Failed to do HTTP call...\n");
        return 0;
    }
}

int CL_Smilo_BetConfirmed(char* uid) {
    printf("Checking if bet is confirmed...\n");

    // Format url to contain query parameter
    char url[1024];
    char* urlTemplate = "v1/client/betconfirmed?uid=%s";
    sprintf(url, urlTemplate, uid);

    // Notify Smilo server agent
    char response[4096];
    if(HTTP_Get("127.0.0.1", url, 8090, response, sizeof(response))) {
        printf("  Agent response: %s\n", response);
        if (!strcmp(response, "true")) {
            printf("  BET CONFIRMED: 1! \n");
            return 1;
        } else {
            printf("  BET CONFIRMED: 0! \n");
            return 0;
        }
    }
    else {
        printf("Failed to do HTTP call...\n");
        return 0;
    }
}

int CL_Smilo_Get_Validated_Player_Count() {
    printf("Get player count!\n");

    // Format url to contain query parameter
    char url[1024];
    char* urlTemplate = "v1/client/validatedPlayersCount%s";
    sprintf(url, urlTemplate, "");

    char response[4096];
    if(HTTP_Get("127.0.0.1", url, 8090, response, sizeof(response))) {
        printf("  Agent response: %s\n", response);
        return atoi(response);
    }
    else {
        printf("Failed to do HTTP call...\n");
        return 0;
    }
}

int CL_Smilo_GameInfo(smilo_game_info* output) {
    printf("Retrieving game info...\n");

    char response[4096];
    if(HTTP_Get("127.0.0.1", "v1/client/gamedetails", 8090, response, sizeof(response))) {
        printf("  Agent response: %s\n", response);

        // Line formatting
        // line 1: input amount
        // line 2: validated contract ('true' or 'false')
        // line 3, 4 and 5: first, second and third place rewards
        int index = 0;
        for(char* p = strtok(response, ","); p != NULL; p = strtok(NULL, ",")) {
            printf("Read line: %s\n", p);
            switch(index) {
                case(0):
                    output->input_amount = strtol(p, (char **)NULL, 10);
                    break;
                case(1):
                    output->valid_smart_contract = strcmp(p, "true") ? 0 : 1;
                    break;
                case(2):
                    output->payout_amounts[0] = strtol(p, (char **)NULL, 10);
                    break;
                case(3):
                    output->payout_amounts[1] = strtol(p, (char **)NULL, 10);
                    break;
                case(4):
                    output->payout_amounts[2] = strtol(p, (char **)NULL, 10);
                    break;
            }

            if(index == 5)
                break;

            index++;
        }

        // Incomplete response
        if(index < 5)
            return 0;

        return 1;
    }
    else {
        printf("Failed to do HTTP call...\n");
        return 0;
    }
}
