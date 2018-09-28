typedef struct {
    unsigned int input_amount;
    unsigned int payout_amounts[3];
    int valid_smart_contract;
} smilo_game_info;

void CL_Smilo_Connected(char* id, char* contractAddress);

int CL_Smilo_BetConfirmed();

int CL_Smilo_GameInfo(smilo_game_info* output);
