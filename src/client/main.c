/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
// cl_main.c  -- client main loop

#include "client.h"

cvar_t  *rcon_address;

cvar_t  *cl_noskins;
cvar_t  *cl_footsteps;
cvar_t  *cl_timeout;
cvar_t  *cl_predict;
cvar_t  *cl_predict_crouch;
cvar_t  *cl_gun;
cvar_t  *cl_gunalpha;
cvar_t  *cl_gunfov;
cvar_t  *cl_gun_x;
cvar_t  *cl_gun_y;
cvar_t  *cl_gun_z;
cvar_t  *cl_warn_on_fps_rounding;
cvar_t  *cl_maxfps;
cvar_t  *cl_async;
cvar_t  *r_maxfps;
cvar_t  *cl_autopause;

cvar_t  *cl_new_movement_sounds;

cvar_t  *cl_kickangles;
cvar_t  *cl_rollhack;
cvar_t  *cl_noglow;
cvar_t  *cl_nolerp;

#if USE_DEBUG
cvar_t  *cl_shownet;
cvar_t  *cl_showmiss;
cvar_t  *cl_showclamp;
#endif

cvar_t  *cl_thirdperson;
cvar_t  *cl_thirdperson_angle;
cvar_t  *cl_thirdperson_range;

cvar_t  *cl_disable_particles;
cvar_t  *cl_disable_explosions;
cvar_t  *cl_chat_notify;
cvar_t  *cl_chat_sound;
cvar_t  *cl_chat_filter;

cvar_t  *cl_disconnectcmd;
cvar_t  *cl_changemapcmd;
cvar_t  *cl_beginmapcmd;

cvar_t  *cl_ignore_stufftext;

cvar_t  *cl_gibs;
#if USE_FPS
cvar_t  *cl_updaterate;
#endif

cvar_t  *cl_protocol;

cvar_t  *gender_auto;

cvar_t  *cl_vwep;

//
// userinfo
//
cvar_t  *info_password;
cvar_t  *info_spectator;
cvar_t  *info_name;
cvar_t  *info_skin;
cvar_t  *info_rate;
cvar_t  *info_fov;
cvar_t  *info_msg;
cvar_t  *info_hand;
cvar_t  *info_gender;
cvar_t  *info_uf;
#if USE_CLIENT
#if USE_AQTION
cvar_t  *info_steamid;
cvar_t  *info_steamcloudappenabled;
cvar_t  *info_steamclouduserenabled;
cvar_t  *cl_mk23_sound;
cvar_t  *cl_mp5_sound;
cvar_t  *cl_m4_sound;
cvar_t  *cl_m3_sound;
cvar_t  *cl_hc_sound;
cvar_t  *cl_ssg_sound;
#endif
#endif
cvar_t  *info_version;

#if USE_REF
extern cvar_t *gl_modulate_world;
extern cvar_t *gl_modulate_entities;
extern cvar_t *gl_brightness;
#endif

client_static_t cls;
client_state_t  cl;

centity_t   cl_entities[MAX_EDICTS];

// used for executing stringcmds
cmdbuf_t    cl_cmdbuf;
char        cl_cmdbuf_text[MAX_STRING_CHARS];

//======================================================================

typedef enum {
    REQ_FREE,
    REQ_STATUS_CL,
    REQ_STATUS_UI,
#if USE_DISCORD && USE_CURL //rekkie -- discord -- s
    REQ_STATUS_DISCORD,
#endif //rekkie -- discord -- e
    REQ_INFO,
    REQ_RCON
} requestType_t;

typedef struct {
    requestType_t type;
    netadr_t adr;
    unsigned time;
} request_t;

#define MAX_REQUESTS    64
#define REQUEST_MASK    (MAX_REQUESTS - 1)

static request_t    clientRequests[MAX_REQUESTS];
static unsigned     nextRequest;

static request_t *CL_AddRequest(const netadr_t *adr, requestType_t type)
{
    request_t *r;

    r = &clientRequests[nextRequest++ & REQUEST_MASK];
    r->adr = *adr;
    r->type = type;
    r->time = cls.realtime;

    return r;
}

static request_t *CL_FindRequest(void)
{
    request_t *r;
    int i, count;

    count = MAX_REQUESTS;
    if (count > nextRequest)
        count = nextRequest;

    // find the most recent request sent to this address
    for (i = 0; i < count; i++) {
        r = &clientRequests[(nextRequest - i - 1) & REQUEST_MASK];
        if (!r->type) {
            continue;
        }
        if (r->adr.type == NA_BROADCAST) {
            if (cls.realtime - r->time > 3000) {
                continue;
            }
            if (!NET_IsLanAddress(&net_from)) {
                continue;
            }
        } else {
            if (cls.realtime - r->time > 6000) {
                break;
            }
            if (!NET_IsEqualBaseAdr(&net_from, &r->adr)) {
                continue;
            }
        }

        return r;
    }

    return NULL;
}

//======================================================================

//=====================================================================================================
#if USE_DISCORD && USE_CURL && USE_AQTION //rekkie -- discord -- s
//=====================================================================================================

//rekkie -- external ip -- s
#include <curl/curl.h>

cvar_t* cl_extern_ip;   // External IP address

static size_t CurlWriteCallback(char* buf, size_t size, size_t nmemb, void* up)
{
    if (strlen(buf) <= 16) // Length of an IP address
    {
        buf[strlen(buf) - 1] = '\0'; // Remove the newline
        cl_extern_ip = Cvar_Set("cl_extern_ip", buf);
    }
    return size * nmemb; // Return how many bytes we read
}
static void CL_GetExternalIP(void)
{
    CURL* curl = curl_easy_init(); // Init the curl session
    curl_easy_setopt(curl, CURLOPT_URL, "http://icanhazip.com"); // URL to get IP
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CurlWriteCallback); // Write the data
    CURLcode res = curl_easy_perform(curl); // Perform the request
    if (res != CURLE_OK) // Something went wrong
    {
        // Failover - switching to a redundant URL
        curl_easy_setopt(curl, CURLOPT_URL, "http://checkip.amazonaws.com/"); // URL to get IP
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CurlWriteCallback); // Write the data
        res = curl_easy_perform(curl); // Perform the request
        if (res != CURLE_OK) // Something went wrong, again!
        {
            Com_Printf("%s [CURL] could not resolve the external IP, curl returned: %i\n", __func__, res);
        }
    }
    
    // If all went well
    Com_Printf("External IP: %s\n", cl_extern_ip->string);
    
    curl_easy_cleanup(curl); // Cleanup
}

// Determine if connection is loopback, private ip, or public ip -- https://en.wikipedia.org/wiki/Private_network
static qboolean CL_IsPrivateNetwork(void)
{
    //--------------------------------------------------------------
    // Loopback
    if (cls.serverAddress.type == NA_LOOPBACK) // Loopback
    {
        return true; // 127.x
    }
    else if (cls.serverAddress.type >= NA_BROADCAST) // LAN, IPv4, IPv6
    {
        qboolean ispv = NET_IsLanAddress(&cls.serverAddress);
        if (ispv) // Private IP
        {
            return true; // (10.x  172.x  192.x  etc)
        }
        else // Public IP
        {
            return false;
        }
    }
    else
        return false;
    //--------------------------------------------------------------
}
//rekkie -- external ip -- e


// Using the Discord API
// https://discord.com/developers/docs/game-sdk/sdk-starter-guide
//
// Requires the Discord Game SDK
// https://dl-game-sdk.discordapp.net/2.5.6/discord_game_sdk.zip
//
// Extract the contents of the zip to /extern/discord/

#include "../extern/discord/c/discord_game_sdk.h"

#if _MSC_VER >= 1920 && !__INTEL_COMPILER
#if defined( _WIN64 )
        #pragma comment(lib, "../extern/discord/lib/x86_64/discord_game_sdk.dll.lib")
    #elif defined( _WIN32 )
        #pragma comment(lib, "../extern/discord/lib/x86/discord_game_sdk.dll.lib")
    #endif // End 64/32 bit check
#elif _MSC_VER < 1920 // older MSC
// Push/Pop fix needed for older versions of Visual Studio to prevent unexpected crashes due to compile configurations
    #pragma pack(push, 8)
        #include "../extern/discord/c/discord_game_sdk.h"
    #pragma pack(pop)
#endif // _MSC_VER >= 1920 && !__INTEL_COMPILER

#define DISCORD_APP_ID 1002762540247433297  // Discord application ID (also referred to as "Client ID" is the game's unique identifier across Discord)
#define DISCORD_APP_TEXT "AQtion"           // Tooltip name
#define DISCORD_APP_IMAGE "aqtion"          // Rich presence -> art asset -> asset name
#define DISCORD_UPDATE_MSEC 1000            // Time between updates, 1000 = 1 second.
#define DISCORD_ACTIVITY_UPDATE_MSEC 15000  // Time between updates, 1000 = 1 second.

cvar_t *cl_discord;                         // Allow the user to disable Discord features for privacy
cvar_t *cl_discord_id;                      // User ID
cvar_t *cl_discord_username;                // User name
cvar_t *cl_discord_discriminator;           // User's unique discrim ( username#discriminator -> bob#1900 )
cvar_t *cl_discord_avatar;                  // Hash of the user's avatar
cvar_t *cl_discord_accept_join_requests;    // If true automatically accept join request, else let the user decide

enum discord_message_type {
    DISCORD_MSG_NULL,       // Null packet
    DISCORD_MSG_PING,       // Ping -> pong
    DISCORD_MSG_CONNECT,    // Client wants to connect to the game server
    DISCORD_MSG_OWNERSHIP   // Transfer of ownership
};

struct Application {
    struct IDiscordCore* core;
    struct IDiscordApplicationManager* application;
    struct IDiscordUserManager* user;
    struct IDiscordImageManager* images;
    struct IDiscordActivityManager* activities;
    struct IDiscordRelationshipManager* relationships;
    struct IDiscordLobbyManager* lobbies;
    struct IDiscordNetworkManager* network;
    struct IDiscordOverlayManager* overlay;
    struct IDiscordStorageManager* storage;
    struct IDiscordStoreManager* store;
    struct IDiscordVoiceManager* voice;
    struct IDiscordAchievementManager* achievements;
};

typedef struct {
    struct Application app;             // Discord app
    struct DiscordCreateParams params;  // Creation parameters
    
    // Events
    IDiscordCoreEvents* events;                                 // Core events
    struct IDiscordUserEvents users_events;                     // Users
    struct IDiscordActivityEvents activities_events;            // Activities
    struct IDiscordRelationshipEvents relationships_events;     // Relationships
    struct IDiscordLobbyEvents lobbies_events;                  // Lobbies
    struct IDiscordAchievementEvents achievements_events;       // Achievements
    struct IDiscordNetworkEvents* network_events;               // Network
    struct IDiscordOverlayEvents* overlay_events;               // Overlay
    IDiscordStorageEvents* storage_events;                      // Storage
    struct IDiscordStoreEvents* store_events;                   // Store
    struct IDiscordVoiceEvents* voice_events;                   // Voice
    struct IDiscordAchievementEvents* achievement_events;       // Achievements
    
    // Activities
    struct DiscordActivity activity;                // Activities (rich presence)

    // Lobbies
    struct IDiscordLobbyTransaction *transaction;   // Transaction
    struct DiscordLobby lobby;                      // Lobby
    
    // User
    struct DiscordUser user;            // User data (the user's discord id, username, etc)
    
    // Init
    qboolean init;                      // Discord is initialized true/false
    qboolean discord_found;             // If Discord is running

    // Callback
    int result;
    
    // Timers
    int last_discord_runtime;           // Last time (in msec) discord was updated
    int last_activity_time;             // Last time (in msec) activity was updated
    
    char server_hostname[64];           // Cache hostname
    char mapname[MAX_QPATH];            // Cache map
    byte curr_players;                  // How many players currently connected to the server
    byte prev_players;                  // How many players previously connected to the server
    
} discord_t;
discord_t discord;

void     CL_InitDiscord(void);
void     CL_CreateDiscordLobby_f(void);
void     CL_DeleteDiscordLobby(void);
void     CL_RunDiscord(void);
void     CL_ShutdownDiscord(void);


static void DiscordCallback(void* data, enum EDiscordResult result)
{
    //Com_Printf("%s %s\n", __func__, data);
    
    discord.result = result;
    switch (discord.result)
    {
        case DiscordResult_Ok:
        //    Com_Printf("%s DiscordResult_Ok\n", __func__);
            break;
        case DiscordResult_ServiceUnavailable:
            Com_Printf("%s DiscordResult_ServiceUnavailable\n", __func__);
            break;
        case DiscordResult_InvalidVersion:
            Com_Printf("%s DiscordResult_InvalidVersion\n", __func__);
            break;
        case DiscordResult_LockFailed:
            Com_Printf("%s DiscordResult_LockFailed\n", __func__);
            break;
        case DiscordResult_InternalError:
            Com_Printf("%s DiscordResult_InternalError\n", __func__);
            break;
        case DiscordResult_InvalidPayload:
            Com_Printf("%s DiscordResult_InvalidPayload\n", __func__);
            break;
        case DiscordResult_InvalidCommand:
            Com_Printf("%s DiscordResult_InvalidCommand\n", __func__);
            break;
        case DiscordResult_InvalidPermissions:
            Com_Printf("%s DiscordResult_InvalidPermissions\n", __func__);
            break;
        case DiscordResult_NotFetched:
            Com_Printf("%s DiscordResult_NotFetched\n", __func__);
            break;
        case DiscordResult_NotFound:
            Com_Printf("%s DiscordResult_NotFound\n", __func__);
            break;
        case DiscordResult_Conflict:
            Com_Printf("%s DiscordResult_Conflict\n", __func__);
            break;
        case DiscordResult_InvalidSecret:
            Com_Printf("%s DiscordResult_InvalidSecret\n", __func__);
            break;
        case DiscordResult_InvalidJoinSecret:
            Com_Printf("%s DiscordResult_InvalidJoinSecret\n", __func__);
            break;
        case DiscordResult_NoEligibleActivity:
            Com_Printf("%s DiscordResult_NoEligibleActivity\n", __func__);
            break;
        case DiscordResult_InvalidInvite:
            Com_Printf("%s DiscordResult_InvalidInvite\n", __func__);
            break;
        case DiscordResult_NotAuthenticated:
            Com_Printf("%s DiscordResult_NotAuthenticated\n", __func__);
            break;
        case DiscordResult_InvalidAccessToken:
            Com_Printf("%s DiscordResult_InvalidAccessToken\n", __func__);
            break;
        case DiscordResult_ApplicationMismatch:
            Com_Printf("%s DiscordResult_ApplicationMismatch\n", __func__);
            break;
        case DiscordResult_InvalidDataUrl:
            Com_Printf("%s DiscordResult_InvalidDataUrl\n", __func__);
            break;
        case DiscordResult_InvalidBase64:
            Com_Printf("%s DiscordResult_InvalidBase64\n", __func__);
            break;
        case DiscordResult_NotFiltered:
            Com_Printf("%s DiscordResult_NotFiltered\n", __func__);
            break;
        case DiscordResult_LobbyFull:
            Com_Printf("%s DiscordResult_LobbyFull\n", __func__);
            break;
        case DiscordResult_InvalidLobbySecret:
            Com_Printf("%s DiscordResult_InvalidLobbySecret\n", __func__);
            break;
        case DiscordResult_InvalidFilename:
            Com_Printf("%s DiscordResult_InvalidFilename\n", __func__);
            break;
        case DiscordResult_InvalidFileSize:
            Com_Printf("%s DiscordResult_InvalidFileSize\n", __func__);
            break;
        case DiscordResult_InvalidEntitlement:
            Com_Printf("%s DiscordResult_InvalidEntitlement\n", __func__);
            break;
        case DiscordResult_NotInstalled:
            Com_Printf("%s DiscordResult_NotInstalled\n", __func__);
            break;
        case DiscordResult_NotRunning:
            Com_Printf("%s DiscordResult_NotRunning\n", __func__);
            break;
        case DiscordResult_InsufficientBuffer:
            Com_Printf("%s DiscordResult_InsufficientBuffer\n", __func__);
            break;
        case DiscordResult_PurchaseCanceled:
            Com_Printf("%s DiscordResult_PurchaseCanceled\n", __func__);
            break;
        case DiscordResult_InvalidGuild:
            Com_Printf("%s DiscordResult_InvalidGuild\n", __func__);
            break;
        case DiscordResult_InvalidEvent:
            Com_Printf("%s DiscordResult_InvalidEvent\n", __func__);
            break;
        case DiscordResult_InvalidChannel:
            Com_Printf("%s DiscordResult_InvalidChannel\n", __func__);
            break;
        case DiscordResult_InvalidOrigin:
            Com_Printf("%s DiscordResult_InvalidOrigin\n", __func__);
            break;
        case DiscordResult_RateLimited:
            Com_Printf("%s DiscordResult_RateLimited\n", __func__);
            break;
        case DiscordResult_OAuth2Error:
            Com_Printf("%s DiscordResult_OAuth2Error\n", __func__);
            break;
        case DiscordResult_SelectChannelTimeout:
            Com_Printf("%s DiscordResult_SelectChannelTimeout\n", __func__);
            break;
        case DiscordResult_GetGuildTimeout:
            Com_Printf("%s DiscordResult_GetGuildTimeout\n", __func__);
            break;
        case DiscordResult_SelectVoiceForceRequired:
            Com_Printf("%s DiscordResult_SelectVoiceForceRequired\n", __func__);
            break;
        case DiscordResult_CaptureShortcutAlreadyListening:
            Com_Printf("%s DiscordResult_CaptureShortcutAlreadyListening\n", __func__);
            break;
        case DiscordResult_UnauthorizedForAchievement:
            Com_Printf("%s DiscordResult_UnauthorizedForAchievement\n", __func__);
            break;
        case DiscordResult_InvalidGiftCode:
            Com_Printf("%s DiscordResult_InvalidGiftCode\n", __func__);
            break;
        case DiscordResult_PurchaseError:
            Com_Printf("%s DiscordResult_PurchaseError\n", __func__);
            break;
        case DiscordResult_TransactionAborted:
        //    Com_Printf("%s DiscordResult_TransactionAborted\n", __func__);
            break;
        default:
            Com_Printf("%s Unknown error code %i\n", __func__, discord.result);
            break;
    }
}

// Log output - error, warning, information, debug
static void DiscordLogCallback(void* hook_data, enum EDiscordLogLevel level, const char* message)
{
    if (level == DiscordLogLevel_Error)
        Com_Printf("%s ERROR: %s\n", __func__, message);
    else if (level == DiscordLogLevel_Warn)
        Com_Printf("%s WARN: %s\n", __func__, message);
    else if (level == DiscordLogLevel_Info)
        Com_Printf("%s INFO: %s\n", __func__, message);
    else if (level == DiscordLogLevel_Debug)
        Com_Printf("%s DEBUG: %s\n", __func__, message);
}

//  Not used anywhere?
// static void OnOAuth2Token(void* data, enum EDiscordResult result, struct DiscordOAuth2Token* token)
// {
//     if (result == DiscordResult_Ok)
//         Com_Printf("%s access token: %s\n", __func__, token->access_token);
//     else
//     {
//         Com_Printf("%s GetOAuth2Token failed with: ", __func__);
//         DiscordCallback(NULL, result);
//     }
// }

// Filter Discord friends
static bool RelationshipPassFilter(void* data, struct DiscordRelationship* relationship)
{
    return (relationship->type == DiscordRelationshipType_Friend); // Return true if they are a Discord friend
}
// List all Discord friends
static void OnRelationshipsRefresh(void* data)
{
    struct Application* app = (struct Application*)data;
    struct IDiscordRelationshipManager* module = app->relationships;

    module->filter(module, app, RelationshipPassFilter);
    int32_t relation_count = 0;
    DiscordCallback(NULL, module->count(module, &relation_count));
    if (discord.result == DiscordResult_Ok)
    {
        for (int32_t i = 0; i < relation_count; i += 1)
        {
            struct DiscordRelationship relationship;
            DiscordCallback(NULL, module->get_at(module, i, &relationship));
            //if (discord.result == DiscordResult_Ok)
            //    Com_Printf("%lld %s#%s\n", (long long)relationship.user.id, relationship.user.username, relationship.user.discriminator);
        }
    }
}
static void OnRelationshipUpdate(void* data, struct DiscordRelationship* relationship)
{
}

// User event
static void OnUserUpdated(void* data)
{
    struct Application* app = (struct Application*)data;
    struct DiscordUser user;
    discord.app.user->get_current_user(app->user, &user);
    discord.user = user;

    cl_discord_id = Cvar_Set("cl_discord_id", va("%lld", (long long)discord.user.id));
    cl_discord_username = Cvar_Set("cl_discord_username", discord.user.username);
    cl_discord_discriminator = Cvar_Set("cl_discord_discriminator", discord.user.discriminator);
    cl_discord_avatar = Cvar_Set("cl_discord_avatar", discord.user.avatar);
    
    //Com_Printf("%s User profile updated: %lld %s#%s\n", __func__, (long long)discord.user.id, discord.user.username, discord.user.discriminator);
}

// Fires when the user receives a join or spectate invite.
// We let the user decide what they want to do, instead of forcing them to join; this function is currently here just to log the event
static void OnActivityInvite(void* data, enum EDiscordActivityActionType type, struct DiscordUser *user, struct DiscordActivity *activity)
{   
    if (type == DiscordActivityActionType_Join)
    {
        //Com_Printf("%s DiscordActivityActionType_Join %lld %s#%s\n", __func__, (long long)user->id, user->username, user->discriminator);
        
        // If we ever wanted to force the user to join, call this function
        //discord.app.activities->accept_invite(discord.app.activities, user->id, "Accepted", DiscordCallback);
    }
    else // DiscordActivityActionType_Spectate
    {
        //Com_Printf("%s DiscordActivityActionType_Spectate %lld %s#%s\n", __func__, (long long)user->id, user->username, user->discriminator);

        // If we ever wanted to force the user to join, call this function
        //discord.app.activities->accept_invite(discord.app.activities, user->id, "Accepted", DiscordCallback);
    }
}
static void DiscordLobbyCallBack(void* data, enum EDiscordResult result, struct DiscordLobby* lobby)
{
    DiscordCallback("DiscordLobbyCallBack", result); // Check the result
    
    //if (lobby != NULL)
    //    Com_Printf("%s %s ID[%lld] SECRET[%s]\n", __func__, (char*)data, (long long)lobby->id, lobby->secret);
}

// This function runs when a player clicks the join button when invited through Discord
// The secret received is the concatenation of lobby_id + lobby_secret
static void OnActivityJoin(void* event_data, const char* secret) 
{
    //Com_Printf("%s secret[%s]\n", __func__, secret);

    if (discord.lobby.id == 0 && strlen(secret) > 0)
    {
        DiscordLobbyId lobby_id;
        DiscordLobbySecret lobby_secret;
        char activity_secret[128]; // Size here is sizeof DiscordLobbySecret
        Q_strlcpy(activity_secret, secret, sizeof(activity_secret));

        // Copy lobby secret (used if lobby ownership is transferred to us)
        Q_snprintf(discord.activity.secrets.join, sizeof(discord.activity.secrets.join), "%s", activity_secret);

        // Extract lobby_id and lobby_secret from activity_secret (lobby_id:secret)
        //-------------------------------------------------------------------------
        // Get lobby_id
        char* token = strtok(activity_secret, ":");
        if (token != NULL)
            lobby_id = atoll(token);
        else
            lobby_id = 0;
        
        // Get lobby_secret
        token = strtok(NULL, ":");
        if (token != NULL)
            Q_strlcpy(lobby_secret, token, sizeof(lobby_secret));
        else
            strcpy(lobby_secret, "");
        //-------------------------------------------------------------------------

        if (lobby_id)
        {
            // Connect the user to the lobby (secret must be lobby_id:lobby_secret)
            Q_strlcpy(activity_secret, secret, sizeof(activity_secret));
            discord.app.lobbies->connect_lobby_with_activity_secret(discord.app.lobbies, activity_secret, "OnActivityJoin", DiscordLobbyCallBack);

            if (discord.result == DiscordResult_Ok) // Result from DiscordLobbyCallBack()
            {
                //Com_Printf("%s Connected to lobby %lld\n", __func__, (long long)lobby_id);
                discord.lobby.id = lobby_id;
                Q_strlcpy(discord.lobby.secret, lobby_secret, sizeof(discord.lobby.secret));
            }
        }
    }
}
static void OnActivitySpectate(void* event_data, const char* secret) 
{
    //Com_Printf("%s secret[%s]\n", __func__, secret);
}

// Fires when a user asks to join our game.
static void OnActivityJoinRequest(void* event_data, struct DiscordUser* user) 
{
    //Com_Printf("%s %lld %s#%s\n", __func__, (long long)user->id, user->username, user->discriminator);

    // Here we can either automatically accept a join request by calling up send_request_reply()
    // ... or we can do nothing and let the user decide if they want to accept or deny
    if (cl_discord_accept_join_requests->value)
        discord.app.activities->send_request_reply(discord.app.activities, user->id, DiscordActivityJoinRequestReply_Yes, "Accepted", DiscordCallback);
    else
        ; // let user decide
    
}

static void CL_DiscordParseServerStatus(serverStatus_t* status, const char* string)
{
    const char* s;
    size_t infolen;

    s = Q_strchrnul(string, '\n'); // parse '\n' terminated infostring

    // Due to off-by-one error in the original version of Info_SetValueForKey,
    // some servers produce infostrings up to 512 characters long. Work this
    // bug around by cutting off the last character(s).
    infolen = s - string;
    if (infolen >= MAX_INFO_STRING)
        infolen = MAX_INFO_STRING - 1;

    // copy infostring off
    memcpy(status->infostring, string, infolen);
    status->infostring[infolen] = 0;

    if (!Info_Validate(status->infostring))
        strcpy(status->infostring, "\\hostname\\badinfo");

    Q_snprintf(discord.server_hostname, sizeof(discord.server_hostname), "%s", Info_ValueForKey(string, "hostname"));

    // parse player list
    discord.curr_players = 0;
    while (discord.curr_players < MAX_STATUS_PLAYERS)
    {
        COM_Parse(&s); // Score
        COM_Parse(&s); // Ping
        COM_Parse(&s); // Name
        if (!s)
            break;
        discord.curr_players++;
    }
}

// Allow players to invite or join a server via Discord
// A lobby is created when a player joins a server, and deleted when they leave the game
static void CL_DiscordGameInvite(void)
{   
    // Create a lobby if none exists
    if (discord.lobby.id == 0)
    {
        CL_CreateDiscordLobby_f();
    }

    // Always update these
    if (discord.lobby.id)
    {
        Q_snprintf(discord.activity.party.id, sizeof(discord.activity.party.id), "%lld\n", (long long)discord.lobby.id);

        // Join -- the join secret must be a concatenation of lobby id and secret ==> lobby_id:lobby_secret
        Q_snprintf(discord.activity.secrets.join, sizeof(discord.activity.secrets.join), "%lld:%s", (long long)discord.lobby.id, discord.lobby.secret); // Secrets.join must be unique
        //Com_Printf("%s %s\n", __func__, discord.activity.secrets.join);

        discord.activity.party.size.current_size = discord.curr_players;
        discord.activity.party.size.max_size = cl.maxclients;
    }
}

static void CL_UpdateActivity(void)
{
    if (cls.serverAddress.type == NA_UNSPECIFIED)
        return;

    if (cls.demo.playback)
    {
        if (discord.lobby.id) // If not connected, remove the lobby (if any)
            CL_DeleteDiscordLobby();

        discord.last_activity_time = 0;
        discord.server_hostname[0] = '\0';
        sprintf(discord.activity.details, "Watching");
        discord.activity.state[0] = '\0';

        // Since the user isn't in a map, just use game logo
        Q_snprintf(discord.activity.assets.large_image, sizeof(discord.activity.assets.large_image), "%s", DISCORD_APP_IMAGE);
        Q_snprintf(discord.activity.assets.large_text, sizeof(discord.activity.assets.large_text), "%s", DISCORD_APP_TEXT);

        // Reset the small logo because we're using the large logo instead
        discord.activity.assets.small_image[0] = '\0';
        discord.activity.assets.small_text[0] = '\0';
    }
    else if (cls.state == ca_active) // Client remote connection
    {
        // Get the hostname and player count (local and remote games)
        if (discord.last_activity_time < cls.realtime)
        {
            // NOTE: Only update every ~15 seconds because this requests an infostring size string
            // from the server that is up to 512 bytes long.
            discord.last_activity_time = cls.realtime + DISCORD_ACTIVITY_UPDATE_MSEC;

            // Get hostname and player count by sending a request to the game server, 
            // the reply is processed by CL_DiscordParseServerStatus().
            // The result is cached to discord.server_hostname
            netadr_t    adr;
            neterr_t    ret;

            adr = cls.netchan.remote_address;
            CL_AddRequest(&adr, REQ_STATUS_DISCORD);

            NET_Config(NET_CLIENT);

            ret = OOB_PRINT(NS_CLIENT, &adr, "status");
            if (ret == NET_ERROR)
                Com_Printf("%s to %s\n", NET_ErrorString(), NET_AdrToString(&adr));

            return; // Wait for server to respond
        }

        // Apply map name and player count
        if (strlen(cl.mapname) > 0)
        {       
            if (cls.state == ca_active) // Player fully in game
                Q_snprintf(discord.activity.state, sizeof(discord.activity.state), "Playing %s [%i/%i]", cl.mapname, discord.curr_players, cl.maxclients);
            else // Connection: handshake, downloading, loading, etc.
                sprintf(discord.activity.state, "Connecting...");
        }

        // If connection is loopback, ignore users 'hostname' setting, which defaults to 'noname'
        if (cls.serverAddress.type == NA_LOOPBACK)
            sprintf(discord.server_hostname, "Local Game");

        // Hostname
        Q_snprintf(discord.activity.details, sizeof(discord.activity.details), "%s", discord.server_hostname);

        // Add the map image (defaults to app icon if not found)
        Q_snprintf(discord.activity.assets.large_image, sizeof(discord.activity.assets.large_image), "%s", cl.mapname); // Set map image
        Q_snprintf(discord.activity.assets.large_text, sizeof(discord.activity.assets.large_text), "%s", cl.mapname);  // Set map name

        // Add the game logo under the map image
        Q_snprintf(discord.activity.assets.small_image, sizeof(discord.activity.assets.small_image), "%s", DISCORD_APP_IMAGE);
        Q_snprintf(discord.activity.assets.small_text, sizeof(discord.activity.assets.small_text), "%s", DISCORD_APP_TEXT);

        CL_DiscordGameInvite(); // Creates a lobby and opens up game invites
    }
    else // Main menu
    {
        if (discord.lobby.id) // If not connected, remove the lobby (if any)
            CL_DeleteDiscordLobby();
        
        discord.last_activity_time = 0;
        discord.server_hostname[0] = '\0';
        sprintf(discord.activity.details, "Main menu");
        discord.activity.state[0] = '\0';

        // Since the user isn't in a map, just use game logo
        Q_snprintf(discord.activity.assets.large_image, sizeof(discord.activity.assets.large_image), "%s", DISCORD_APP_IMAGE);
        Q_snprintf(discord.activity.assets.large_text, sizeof(discord.activity.assets.large_text), "%s", DISCORD_APP_TEXT);

        // Reset the small logo because we're using the large logo instead
        discord.activity.assets.small_image[0] = '\0';
        discord.activity.assets.small_text[0] = '\0';
    }

    discord.activity.type = DiscordActivityType_Playing;
    discord.activity.application_id = DISCORD_APP_ID;
    discord.activity.timestamps.start = 0;
    discord.activity.timestamps.end = 0;
    discord.activity.instance = true;
    discord.app.activities->update_activity(discord.app.activities, &discord.activity, "update_activity", DiscordCallback);
}

static void OnLobbyUpdate(void* event_data, int64_t lobby_id) 
{
    //Com_Printf("%s lobbyID[%lld]\n", __func__, (long long)lobby_id);
}
static void OnLobbyDelete(void* event_data, int64_t lobby_id, uint32_t reason) 
{
    //Com_Printf("%s lobbyID[%lld] reason[%i]\n", __func__, (long long)lobby_id, reason);
}
static void OnLobbyConnect(void* data, enum EDiscordResult result, struct DiscordLobby* lobby)
{
    if (result == DiscordResult_Ok)
    {
        //Com_Printf("%s lobby_id[%lld] lobby_type[%i] owner_id[%lld] secret[%s] capacity[%i] locked[%i]\n", __func__, (long long)lobby->id, lobby->type, (long long)lobby->owner_id, lobby->secret, lobby->capacity, lobby->locked);
        discord.lobby.id = lobby->id;
        discord.lobby.type = lobby->type;
        discord.lobby.owner_id = lobby->owner_id;
        Q_snprintf(discord.lobby.secret, sizeof(discord.lobby.secret), "%s", lobby->secret);
        discord.lobby.capacity = lobby->capacity;
        discord.lobby.locked = lobby->locked;
    }
    else
    {
        Com_Printf("%s OnLobbyConnect failed with: ", __func__);
        DiscordCallback(NULL, result);
    }
}
static void OnLobbyMemberConnect(void* event_data, int64_t lobby_id, int64_t user_id)
{
    char addr[32]; // IP + Port
    char msg[256];

    //Com_Printf("%s lobby_id[%lld] user_id[%lld]\n", __func__, (long long)lobby_id, (long long)user_id);

    // If the host is using a private network IP, ensure the invited client connects to the host's public IP
    if (CL_IsPrivateNetwork()) // Private ip
    {
        
        if (strlen(cl_extern_ip->string)) // We have a valid external IP
        {
            // Copy the external ip + port
            Q_snprintf(addr, sizeof(addr), "%s:%s", cl_extern_ip->string, net_port->string);
        }
        else
            return; // No external IP, or couldn't connect to http://icanhazip.com to get our IP
    }
    else // Public IP - Just use the server IP + Port
    {
        // Copy the remote server ip + port
        Q_snprintf(addr, sizeof(addr), "%s", NET_AdrToString(&cls.serverAddress));
    }
    
    //Com_Printf("%s %s\n", __func__, addr);
    
    // Send new member the server connection details
    // We sent the user_id to identify who the message is for
    // If the game is password protected
    if (strlen(info_password->string) > 0)
    {
        // Send msg type + intended recipient + ip:port + game password
        Q_snprintf(msg, sizeof(msg) - 1, "%i|%lld|%s|%s|", DISCORD_MSG_CONNECT, (long long)user_id, addr, info_password->string);
    }
    else
    {
        // Send msg type + intended recipient + ip:port + empty pass (no password)
        Q_snprintf(msg, sizeof(msg) - 1, "%i|%lld|%s| |", DISCORD_MSG_CONNECT, (long long)user_id, addr);
    }
    
    //Com_Printf("%s [%s]\n", __func__, msg);

    // Broadcast
    discord.app.lobbies->send_lobby_message(discord.app.lobbies, discord.lobby.id, (uint8_t*)msg, strlen(msg), "OnLobbyMemberConnect", DiscordCallback);
}
static void OnLobbyMemberUpdate(void* event_data, int64_t lobby_id, int64_t user_id) 
{
    //Com_Printf("%s lobby_id[%lld] user_id[%lld]\n", __func__, (long long)lobby_id, (long long)user_id);
}
static void OnLobbyMemberDisconnect(void* event_data, int64_t lobby_id, int64_t user_id) 
{
    //Com_Printf("%s lobby_id[%lld] user_id[%lld]\n", __func__, (long long)lobby_id, (long long)user_id);
}
static void OnLobbyMessage(void* event_data, int64_t lobby_id, int64_t user_id, uint8_t* data, uint32_t data_length) 
{
    if (data_length == 0 || data_length > 1024)
        return;

    int64_t intended_user_id = 0;
    
    char msg[1024];
    Q_strlcpy(msg, (char*)data, data_length);
    
    //Com_Printf("%s lobby_id[%lld] user_id[%lld] data[%s] data_length[%i]\n", __func__, (long long)lobby_id, (long long)user_id, msg, data_length);

    int msg_len = 0;
    enum discord_message_type msg_type = 0;
        
    // Get the message type
    char* token = strtok(msg, "|");
    if (token)
    {
        msg_type = atoi(token);
        msg_len += strlen(token) + 1;
    }
    else
    {
        Com_Printf("%s received an invalid message type (malformed data)\n", __func__);
        return;
    }

    // Get the intended user_id
    token = strtok(NULL, "|");
    if (token)
    {
        intended_user_id = atoll(token);
        msg_len += strlen(token) + 1;
    }
    else
    {
        Com_Printf("%s received an invalid user_id (malformed data)\n", __func__);
        return;
    }

    // Special case - we disconnect from lobby only after we've fully transferred ownership
    if (msg_type == DISCORD_MSG_OWNERSHIP && user_id == discord.user.id)
    {
        //Com_Printf("%s We've successfully transfered ownership of the lobby to [%lld]\n", __func__, (long long)intended_user_id);
        discord.app.lobbies->disconnect_lobby(discord.app.lobbies, lobby_id, "disconnect_lobby", DiscordCallback);
    }

    // Check if the msg was intended for someone else
    if (discord.user.id != intended_user_id)
        return;

    // We've just received ownership of the lobby
    if (msg_type == DISCORD_MSG_OWNERSHIP)
    {
        //Com_Printf("%s You've taken ownership of the lobby\n", __func__);
        
        discord.lobby.id = lobby_id;
        Q_snprintf(discord.activity.party.id, sizeof(discord.activity.party.id), "%lld\n", (long long)discord.lobby.id);
        discord.activity.party.size.current_size = discord.curr_players;
        discord.activity.party.size.max_size = cl.maxclients;
        
        return;
    }

    // We recevied a request to connect to a quake2 server
    if (msg_type == DISCORD_MSG_CONNECT && cls.serverAddress.type < NA_IP) // Not connected
    {
        // Extract server addr and password from the message
        //--------------------------------------------------
        char addr[64];
        char pass[64];
        pass[0] = '\n';

        // Copy the ip:port
        token = strtok(NULL, "|");
        if (token)
        {
            Q_snprintf(addr, sizeof(addr), "%s", token);
            msg_len += strlen(token) + 1;
        }
        else
        {
            Com_Printf("%s received invalid server ip:port (malformed data)\n", __func__);
            return;
        }

        // Copy the password
        token = strtok(NULL, "|");
        if (token)
        {
            Q_snprintf(pass, sizeof(pass), "%s", token);
            if (strcmp(pass, " ") != 0) // Has a password, and not " " empty space
            {
                Cvar_Set("password", pass); // Set server password
                //Com_Printf("%s addr[%s] pass[%s]\n", __func__, addr, pass);
            }
            else
            {
                //Com_Printf("%s addr[%s]\n", __func__, addr);
            }
        }
        //---------------------------------------------------------

        //if (pass[0] != '\0') // Set the game password, if any
        //    Cvar_Set("password", pass);

        // Connect player to server
        netadr_t address;
        int protocol;
        protocol = cl_protocol->integer;
        if (!protocol) {
            protocol = PROTOCOL_VERSION_Q2PRO;
        }
        if (!NET_StringToAdr(addr, &address, PORT_SERVER)) {
            Com_Printf("Bad server address\n");
            return;
        }
        Q_strlcpy(cls.servername, addr, sizeof(cls.servername)); // copy early to avoid potential cmd_argv[1] clobbering
        SV_Shutdown("Server was killed.\n", ERR_DISCONNECT); // if running a local server, kill it and reissue
        NET_Config(NET_CLIENT);
        CL_Disconnect(ERR_RECONNECT);
        cls.serverAddress = address;
        cls.serverProtocol = protocol;
        cls.protocolVersion = 0;
        cls.passive = false;
        cls.state = ca_challenging;
        cls.connect_time -= CONNECT_FAST;
        cls.connect_count = 0;
        Con_Popup(true);
        CL_CheckForResend();

        return;
    }
}
static void OnLobbySpeaking(void* event_data, int64_t lobby_id, int64_t user_id, bool speaking) 
{
    //Com_Printf("%s lobby_id[%lld] user_id[%lld] speaking[%i]\n", __func__, (long long)lobby_id, (long long)user_id, speaking);
}
static void OnLobbyNetworkMessage(void* event_data, int64_t lobby_id, int64_t user_id, uint8_t channel_id, uint8_t* data, uint32_t data_length)
{
    //Com_Printf("%s lobby_id[%lld] user_id[%lld] channel_id[%i] data[%s] data_length[%i]\n", __func__, (long long)lobby_id, (long long)user_id, channel_id, data, data_length);
}

// Build a list of lobbies
// Note: must run a query before using lobby functions (discord.app.lobbies->func)
static void CL_QueryLobby(void)
{
    struct IDiscordLobbySearchQuery* query;
    discord.app.lobbies->get_search_query(discord.app.lobbies, &query);
    query->limit(query, 1);
    discord.app.lobbies->search(discord.app.lobbies, query, "lobbies->search", DiscordCallback);
}

void CL_CreateDiscordLobby_f(void)
{
    // Call CL_QueryLobby to build a list of lobbies before creating a new lobby
    CL_QueryLobby();
    if (discord.result == DiscordResult_Ok) // Result from CL_QueryLobby() -> search()
    {
        // Search for lobbies
        int32_t lobby_count;
        discord.app.lobbies->lobby_count(discord.app.lobbies, &lobby_count);
        if (lobby_count == 0) // If no existing lobbies
        {
            DiscordCallback(NULL, discord.app.lobbies->get_lobby_create_transaction(discord.app.lobbies, &discord.transaction));
            if (discord.result == DiscordResult_Ok) // If get_lobby_create_transaction was okay
            {
                // Setup lobby
                DiscordMetadataKey key = "quake2";
                DiscordMetadataValue value = "rocks";
                discord.transaction->set_metadata(discord.transaction, key, value);                 // Metadata
                discord.transaction->set_capacity(discord.transaction, cl.maxclients);              // Capacity
                discord.transaction->set_type(discord.transaction, DiscordLobbyType_Public);        // Lobby type (DiscordLobbyType_Private, DiscordLobbyType_Public)
                discord.transaction->set_locked(discord.transaction, false);                        // Locked

                // Create lobby
                discord.app.lobbies->create_lobby(discord.app.lobbies, discord.transaction, "create_lobby", OnLobbyConnect);
            }
        }
    }
}

void CL_DeleteDiscordLobby(void)
{
    if (discord.init && discord.lobby.id)
    {
        // Call CL_QueryLobby to build a list of lobbies before we delete it
        CL_QueryLobby();
        if (discord.result == DiscordResult_Ok)  // Result from CL_QueryLobby() -> search()
        {
            // Search for lobbies to destroy
            int32_t lobby_count = 0;
            DiscordLobbyId lobby_id;
            discord.app.lobbies->lobby_count(discord.app.lobbies, &lobby_count);
            if (lobby_count == 1)
            {
                // Get lobby id
                discord.app.lobbies->get_lobby_id(discord.app.lobbies, 0, &lobby_id);

                //Com_Printf("%s Ownership owner_id[%lld] user.id[%lld]\n", __func__, (long long)discord.lobby.owner_id, (long long)discord.user.id);

                // Transfer ownership of lobby
                if (discord.lobby.owner_id == discord.user.id) // If we own the lobby
                {
                    // Find a lobby member to transfer ownership to
                    int32_t member_count = 0;
                    DiscordUserId user_id = 0;
                    discord.app.lobbies->member_count(discord.app.lobbies, lobby_id, &member_count);
                    //Com_Printf("%s member_count %i\n", __func__, member_count);
                    for (int i = 0; i < member_count; i++)
                    {                       
                        discord.app.lobbies->get_member_user_id(discord.app.lobbies, lobby_id, i, &user_id);
                        //Com_Printf("%s found user_id %lld\n", __func__, (long long)user_id);
                        if (user_id && user_id != discord.user.id)
                            break;
                        else
                            user_id = 0;
                    }
                    
                    // If we found a member (other than ourself) to transfer ownership to
                    if (user_id)
                    {
                        //Com_Printf("%s Transferring ownership of lobby %lld from %lld to %lld\n", __func__, (long long)lobby_id, (long long)discord.user.id, (long long)user_id);
                        
                        discord.app.lobbies->get_lobby_update_transaction(discord.app.lobbies, lobby_id, &discord.transaction);
                        //if (discord.result == DiscordResult_Ok) // If get_lobby_update_transaction was okay
                        {
                            discord.transaction->set_owner(discord.transaction, user_id); // Transfer ownership
                            discord.app.lobbies->update_lobby(discord.app.lobbies, lobby_id, discord.transaction, "update_lobby", DiscordCallback); // Update lobby
                        
                            // Broadcast the change to the lobby
                            char msg[256];
                            Q_snprintf(msg, sizeof(msg) - 1, "%i|%lld|", DISCORD_MSG_OWNERSHIP, (long long)user_id);
                            discord.app.lobbies->send_lobby_message(discord.app.lobbies, discord.lobby.id, (uint8_t*)msg, strlen(msg), "Transfer ownership", DiscordCallback);
                        }
                    }
                    else // If no one else is in the lobby, delete it
                    {
                        // Destroy lobby
                        discord.app.lobbies->delete_lobby(discord.app.lobbies, lobby_id, "delete_lobby", DiscordCallback);
                        //Com_Printf("%s lobby[%lld] destroyed\n", __func__, (long long)lobby_id);
                    }
                }
 
                // Clear mem
                memset(&discord.transaction, 0, sizeof(discord.transaction));
                memset(&discord.lobby, 0, sizeof(discord.lobby));
            }
        }
    }
}

void CL_InitDiscord(void)
{
    Com_Printf("==== %s ====\n", __func__);

    // Creation
    memset(&discord.app, 0, sizeof(discord.app));
    memset(&discord.params, 0, sizeof(discord.params));

    // Events
    memset(&discord.users_events, 0, sizeof(discord.users_events));
    memset(&discord.activities_events, 0, sizeof(discord.activities_events));
    memset(&discord.relationships_events, 0, sizeof(discord.relationships_events));

    // Activities + Lobbies
    memset(&discord.activity, 0, sizeof(discord.activity));
    memset(&discord.transaction, 0, sizeof(discord.transaction));
    memset(&discord.lobby, 0, sizeof(discord.lobby));
    
    // On events
    discord.relationships_events.on_refresh = OnRelationshipsRefresh;
    discord.relationships_events.on_relationship_update = OnRelationshipUpdate;
    discord.users_events.on_current_user_update = OnUserUpdated;
    discord.activities_events.on_activity_invite = OnActivityInvite;
    discord.activities_events.on_activity_join = OnActivityJoin;
    discord.activities_events.on_activity_spectate = OnActivitySpectate;
    discord.activities_events.on_activity_join_request = OnActivityJoinRequest;
    discord.lobbies_events.on_lobby_update = OnLobbyUpdate;
    discord.lobbies_events.on_lobby_delete = OnLobbyDelete;
    discord.lobbies_events.on_member_connect = OnLobbyMemberConnect;
    discord.lobbies_events.on_member_update = OnLobbyMemberUpdate;
    discord.lobbies_events.on_member_disconnect = OnLobbyMemberDisconnect;
    discord.lobbies_events.on_lobby_message = OnLobbyMessage;
    discord.lobbies_events.on_speaking = OnLobbySpeaking;
    discord.lobbies_events.on_network_message = OnLobbyNetworkMessage;
    
    // Creation + Params
    DiscordCreateParamsSetDefault(&discord.params);
    discord.params.client_id = DISCORD_APP_ID;
    discord.params.flags = DiscordCreateFlags_NoRequireDiscord; // Does not require Discord to be running, use this on other platforms
    discord.params.event_data = &discord.app;
    discord.params.events = discord.events;
    discord.params.user_events = &discord.users_events;
    discord.params.activity_events = &discord.activities_events;
    discord.params.relationship_events = &discord.relationships_events;
    discord.params.lobby_events = &discord.lobbies_events;
    discord.params.network_events = discord.network_events;
    discord.params.overlay_events = discord.overlay_events;
    discord.params.storage_events = discord.storage_events;
    discord.params.store_events = discord.store_events;
    discord.params.voice_events = discord.voice_events;
    discord.params.achievement_events = discord.achievement_events;
    
    DiscordCallback(NULL, DiscordCreate(DISCORD_VERSION, &discord.params, &discord.app.core));

    if (discord.result == DiscordResult_Ok)
    {
        // Managers
        discord.app.application = discord.app.core->get_application_manager(discord.app.core);
        discord.app.user = discord.app.core->get_user_manager(discord.app.core);
        discord.app.images = discord.app.core->get_image_manager(discord.app.core);
        discord.app.activities = discord.app.core->get_activity_manager(discord.app.core);
        discord.app.relationships = discord.app.core->get_relationship_manager(discord.app.core);
        discord.app.lobbies = discord.app.core->get_lobby_manager(discord.app.core);
        discord.app.network = discord.app.core->get_network_manager(discord.app.core);
        discord.app.overlay = discord.app.core->get_overlay_manager(discord.app.core);
        discord.app.storage = discord.app.core->get_storage_manager(discord.app.core);
        discord.app.store = discord.app.core->get_store_manager(discord.app.core);
        discord.app.voice = discord.app.core->get_voice_manager(discord.app.core);
        discord.app.achievements = discord.app.core->get_achievement_manager(discord.app.core);

        // Steam
        discord.app.activities->register_steam(discord.app.activities, 1978800); // Aqtion Steam application ID

        // Command line
        //discord.app.activities->register_command(discord.app.activities, "action.exe +set game action");
        //discord.app.activities->register_command(discord.app.activities, "steam://run/1978800/connect/127.0.0.1:27910");

        // Logs
        discord.app.core->set_log_hook(discord.app.core, DiscordLogLevel_Debug, NULL /*void* hook_data*/, DiscordLogCallback);

        // Init
        discord.init = true;
        discord.server_hostname[0] = '\0';
        discord.last_discord_runtime = cls.realtime + DISCORD_UPDATE_MSEC;
        discord.last_activity_time = 0;
    }
    else // Could not connect to the discord network, or the discord app isn't running
    {
        // Init
        discord.init = false;
        discord.discord_found = false;
        discord.server_hostname[0] = '\0';
        discord.last_discord_runtime = 0;
        discord.last_activity_time = 0;

        Com_Printf("%s could not be initialized because Discord is not running\n", __func__);
    }
}

void CL_RunDiscord() // Run in main loop
{
    // Discord cvar disabled, or not running and connected to the Discord network
    if (cl_discord->value != 1 || discord.discord_found == false)
    {
        // If Discord was initialized previously, shut it down
        if (discord.init)
            CL_ShutdownDiscord();
        
        return;
    }
        
    // Ensure Discord is initialized
    if (discord.init == false)
    {
        CL_InitDiscord();
        return; // Give it time to fully init
    }

    // Run discord integration 
    if (cl_discord->value)
    {
        // Run discord callbacks. Must be run first, as per https://discord.com/developers/docs/game-sdk/networking#flush-vs-runcallbacks
        discord.app.core->run_callbacks(discord.app.core);
        
        if (discord.last_discord_runtime < cls.realtime)
        {
            // Update timer
            discord.last_discord_runtime = cls.realtime + DISCORD_UPDATE_MSEC;
            
            // Run discord callbacks. Must be run first, as per https://discord.com/developers/docs/game-sdk/networking#flush-vs-runcallbacks
            //discord.app.core->run_callbacks(discord.app.core);
            
            // Run activity
            CL_UpdateActivity();

            // Flushes the network. Must be run last, and after all networking messages.
            discord.app.network->flush(discord.app.network);
        }
    }
    else // Shutdown discord integration
    {
        CL_ShutdownDiscord();
    }
}

static void CL_ClearDiscordAcivity(void)
{
    discord.app.activities->clear_activity(discord.app.activities, "clear_activity", DiscordCallback);
    memset(&discord.activity, 0, sizeof(discord.activity));
}

void CL_ShutdownDiscord()
{
    Com_Printf("==== %s ====\n", __func__);
    CL_ClearDiscordAcivity();
    CL_DeleteDiscordLobby();

    discord.app.core->run_callbacks(discord.app.core);
    discord.app.core->destroy(discord.app.core);
    discord.discord_found = true;
    discord.init = false;
}
//=====================================================================================================
#endif //rekkie -- discord -- e
//=====================================================================================================

static void CL_UpdateGunSetting(void)
{
    int nogun;

    if (cls.netchan.protocol < PROTOCOL_VERSION_R1Q2) {
        return;
    }

    if (cl_gun->integer == -1) {
        nogun = 2;
    } else if (cl_gun->integer == 0 || (info_hand->integer == 2 && cl_gun->integer == 1)) {
        nogun = 1;
    } else {
        nogun = 0;
    }

    MSG_WriteByte(clc_setting);
    MSG_WriteShort(CLS_NOGUN);
    MSG_WriteShort(nogun);
    MSG_FlushTo(&cls.netchan.message);
}

static void CL_UpdateGibSetting(void)
{
    if (cls.netchan.protocol != PROTOCOL_VERSION_Q2PRO && cls.netchan.protocol != PROTOCOL_VERSION_AQTION) {
        return;
    }

    MSG_WriteByte(clc_setting);
    MSG_WriteShort(CLS_NOGIBS);
    MSG_WriteShort(!cl_gibs->integer);
    MSG_FlushTo(&cls.netchan.message);
}

static void CL_UpdateFootstepsSetting(void)
{
    if (cls.netchan.protocol != PROTOCOL_VERSION_Q2PRO && cls.netchan.protocol != PROTOCOL_VERSION_AQTION) {
        return;
    }

    MSG_WriteByte(clc_setting);
    MSG_WriteShort(CLS_NOFOOTSTEPS);
    MSG_WriteShort(!cl_footsteps->integer);
    MSG_FlushTo(&cls.netchan.message);
}

static void CL_UpdatePredictSetting(void)
{
    if (cls.netchan.protocol != PROTOCOL_VERSION_Q2PRO && cls.netchan.protocol != PROTOCOL_VERSION_AQTION) {
        return;
    }

    MSG_WriteByte(clc_setting);
    MSG_WriteShort(CLS_NOPREDICT);
    MSG_WriteShort(!cl_predict->integer);
    MSG_FlushTo(&cls.netchan.message);
}

#if USE_FPS
static void CL_UpdateRateSetting(void)
{
    if (cls.netchan.protocol != PROTOCOL_VERSION_Q2PRO && cls.netchan.protocol != PROTOCOL_VERSION_AQTION) {
        return;
    }

    MSG_WriteByte(clc_setting);
    MSG_WriteShort(CLS_FPS);
    MSG_WriteShort(cl_updaterate->integer);
    MSG_FlushTo(&cls.netchan.message);
}
#endif

void CL_UpdateRecordingSetting(void)
{
    int rec;

    if (cls.netchan.protocol < PROTOCOL_VERSION_R1Q2) {
        return;
    }

    if (cls.demo.recording) {
        rec = 1;
    } else {
        rec = 0;
    }

#if USE_CLIENT_GTV
    if (cls.gtv.state == ca_active) {
        rec |= 1;
    }
#endif

    MSG_WriteByte(clc_setting);
    MSG_WriteShort(CLS_RECORDING);
    MSG_WriteShort(rec);
    MSG_FlushTo(&cls.netchan.message);
}

/*
===================
CL_ClientCommand
===================
*/
void CL_ClientCommand(const char *string)
{
    if (!cls.netchan.protocol) {
        return;
    }

    Com_DDPrintf("%s: %s\n", __func__, string);

    MSG_WriteByte(clc_stringcmd);
    MSG_WriteString(string);
    MSG_FlushTo(&cls.netchan.message);
}

/*
===================
CL_ForwardToServer

adds the current command line as a clc_stringcmd to the client message.
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
===================
*/
bool CL_ForwardToServer(void)
{
    char    *cmd;

    cmd = Cmd_Argv(0);
    if (cls.state != ca_active || *cmd == '-' || *cmd == '+') {
        return false;
    }

    CL_ClientCommand(Cmd_RawArgsFrom(0));
    return true;
}

/*
==================
CL_ForwardToServer_f
==================
*/
static void CL_ForwardToServer_f(void)
{
    if (cls.state < ca_connected) {
        Com_Printf("Can't \"%s\", not connected\n", Cmd_Argv(0));
        return;
    }

    if (cls.demo.playback) {
        return;
    }

    // don't forward the first argument
    if (Cmd_Argc() > 1) {
        CL_ClientCommand(Cmd_RawArgs());
    }
}

/*
==================
CL_Pause_f
==================
*/
static void CL_Pause_f(void)
{
#if USE_MVD_CLIENT
    if (sv_running->integer == ss_broadcast) {
        Cbuf_InsertText(&cmd_buffer, "mvdpause @@\n");
        return;
    }
#endif

    // activate manual pause
    if (cl_paused->integer == 2) {
        Cvar_Set("cl_paused", "0");
    } else {
        Cvar_Set("cl_paused", "2");
    }

    CL_CheckForPause();
}

/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out
=================
*/
void CL_CheckForResend(void)
{
    char tail[MAX_QPATH];
    char userinfo[MAX_INFO_STRING];
    int maxmsglen;

    if (cls.demo.playback) {
        return;
    }

    // if the local server is running and we aren't
    // then connect
    if (cls.state < ca_connecting && sv_running->integer > ss_loading) {
        strcpy(cls.servername, "localhost");
        cls.serverAddress.type = NA_LOOPBACK;
        cls.serverProtocol = cl_protocol->integer;
        if (cls.serverProtocol < PROTOCOL_VERSION_DEFAULT ||
            cls.serverProtocol > PROTOCOL_VERSION_AQTION) {
			#ifdef AQTION_EXTENSION
            cls.serverProtocol = PROTOCOL_VERSION_AQTION;
			#else
			cls.serverProtocol = PROTOCOL_VERSION_Q2PRO;
			#endif
        }

        // we don't need a challenge on the localhost
        cls.state = ca_connecting;
        cls.connect_time -= CONNECT_FAST;
        cls.connect_count = 0;

        cls.passive = false;

        Con_Popup(true);
        UI_OpenMenu(UIMENU_NONE);
    }

    // resend if we haven't gotten a reply yet
    if (cls.state != ca_connecting && cls.state != ca_challenging) {
        return;
    }

    if (cls.realtime - cls.connect_time < CONNECT_DELAY) {
        return;
    }

    cls.connect_time = cls.realtime;    // for retransmit requests
    cls.connect_count++;

    if (cls.state == ca_challenging) {
        Com_Printf("Requesting challenge... %i\n", cls.connect_count);
        OOB_PRINT(NS_CLIENT, &cls.serverAddress, "getchallenge\n");
        return;
    }

    //
    // We have gotten a challenge from the server, so try and connect.
    //
    Com_Printf("Requesting connection... %i\n", cls.connect_count);

    cls.userinfo_modified = 0;

    // use maximum allowed msglen for loopback
    maxmsglen = net_maxmsglen->integer;
    if (NET_IsLocalAddress(&cls.serverAddress)) {
        maxmsglen = MAX_PACKETLEN_WRITABLE;
    }

    // add protocol dependent stuff
    switch (cls.serverProtocol) {
    case PROTOCOL_VERSION_R1Q2:
        Q_snprintf(tail, sizeof(tail), " %d %d",
                   maxmsglen, PROTOCOL_VERSION_R1Q2_CURRENT);
        cls.quakePort = net_qport->integer & 0xff;
        break;
    case PROTOCOL_VERSION_Q2PRO:
        Q_snprintf(tail, sizeof(tail), " %d %d %d %d",
                   maxmsglen, net_chantype->integer, USE_ZLIB,
                   PROTOCOL_VERSION_Q2PRO_CURRENT);
        cls.quakePort = net_qport->integer & 0xff;
        break;
	case PROTOCOL_VERSION_AQTION:
		Q_snprintf(tail, sizeof(tail), " %d %d %d %d",
			maxmsglen, net_chantype->integer, USE_ZLIB,
			PROTOCOL_VERSION_AQTION_CURRENT);
		cls.quakePort = net_qport->integer & 0xff;
		break;
    default:
        tail[0] = 0;
        cls.quakePort = net_qport->integer;
        break;
    }

    Cvar_BitInfo(userinfo, CVAR_USERINFO);
    Netchan_OutOfBand(NS_CLIENT, &cls.serverAddress,
                      "connect %i %i %i \"%s\"%s\n", cls.serverProtocol, cls.quakePort,
                      cls.challenge, userinfo, tail);
}

static void CL_RecentIP_g(genctx_t *ctx)
{
    netadr_t *a;
    int i, j;

    j = cls.recent_head - RECENT_ADDR;
    if (j < 0) {
        j = 0;
    }
    for (i = cls.recent_head - 1; i >= j; i--) {
        a = &cls.recent_addr[i & RECENT_MASK];
        if (a->type) {
            Prompt_AddMatch(ctx, NET_AdrToString(a));
        }
    }
}

static void CL_Connect_c(genctx_t *ctx, int argnum)
{
    if (argnum == 1) {
        CL_RecentIP_g(ctx);
        Com_Address_g(ctx);
    }
}

/*
================
CL_Connect_f

================
*/
static void CL_Connect_f(void)
{
    char    *server, *p;
    netadr_t    address;

    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: %s <server>\n", Cmd_Argv(0));
        return;
    }

    if (Cmd_Argc() > 2) {
        Com_Printf("Second argument to `%s' is now ignored. "
                   "Set protocol via `cl_protocol' variable.\n", Cmd_Argv(0));
    }

    server = Cmd_Argv(1);

    // support quake2://<address>[/] scheme
    if (!Q_strncasecmp(server, "quake2://", 9)) {
        server += 9;
        if ((p = strchr(server, '/')) != NULL) {
            *p = 0;
        }
    }

    if (!NET_StringToAdr(server, &address, PORT_SERVER)) {
        Com_Printf("Bad server address\n");
        return;
    }

    // copy early to avoid potential cmd_argv[1] clobbering
    Q_strlcpy(cls.servername, server, sizeof(cls.servername));

    // if running a local server, kill it and reissue
    SV_Shutdown("Server was killed.\n", ERR_DISCONNECT);

    NET_Config(NET_CLIENT);

    CL_Disconnect(ERR_RECONNECT);

    cls.serverAddress = address;
    cls.serverProtocol = cl_protocol->integer;
    cls.passive = false;
    cls.state = ca_challenging;
    cls.connect_time -= CONNECT_FAST;
    cls.connect_count = 0;

    Con_Popup(true);

    CL_CheckForResend();

    Cvar_Set("timedemo", "0");
}

static void CL_FollowIP_f(void)
{
    netadr_t *a;
    int i, j;

    if (Cmd_Argc() > 1) {
        // optional second argument references less recent address
        j = atoi(Cmd_Argv(1)) + 1;
        clamp(j, 1, RECENT_ADDR);
    } else {
        j = 1;
    }

    i = cls.recent_head - j;
    if (i < 0) {
        Com_Printf("No IP address to follow.\n");
        return;
    }

    a = &cls.recent_addr[i & RECENT_MASK];
    if (a->type) {
        char *s = NET_AdrToString(a);
        Com_Printf("Following %s...\n", s);
        Cbuf_InsertText(cmd_current, va("connect %s\n", s));
    }
}

static void CL_PassiveConnect_f(void)
{
    netadr_t address;

    if (cls.passive) {
        cls.passive = false;
        Com_Printf("No longer listening for passive connections.\n");
        return;
    }

    // if running a local server, kill it and reissue
    SV_Shutdown("Server was killed.\n", ERR_DISCONNECT);

    NET_Config(NET_CLIENT);

    CL_Disconnect(ERR_RECONNECT);

    if (!NET_GetAddress(NS_CLIENT, &address)) {
        return;
    }

    cls.passive = true;
    Com_Printf("Listening for passive connections at %s.\n",
               NET_AdrToString(&address));
}

void CL_SendRcon(const netadr_t *adr, const char *pass, const char *cmd)
{
    NET_Config(NET_CLIENT);

    CL_AddRequest(adr, REQ_RCON);

    Netchan_OutOfBand(NS_CLIENT, adr, "rcon \"%s\" %s", pass, cmd);
}


/*
=====================
CL_Rcon_f

  Send the rest of the command line over as
  an unconnected command.
=====================
*/
static void CL_Rcon_f(void)
{
    netadr_t    address;

    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: %s <command>\n", Cmd_Argv(0));
        return;
    }

    if (!rcon_password->string[0]) {
        Com_Printf("You must set 'rcon_password' before "
                   "issuing an rcon command.\n");
        return;
    }

    address = cls.netchan.remote_address;
    if (!address.type) {
        if (!rcon_address->string[0]) {
            Com_Printf("You must either be connected, "
                       "or set the 'rcon_address' cvar "
                       "to issue rcon commands.\n");
            return;
        }
        if (!NET_StringToAdr(rcon_address->string, &address, PORT_SERVER)) {
            Com_Printf("Bad address: %s\n", rcon_address->string);
            return;
        }
    }

    CL_SendRcon(&address, rcon_password->string, COM_StripQuotes(Cmd_RawArgs()));
}

static void CL_Rcon_c(genctx_t *ctx, int argnum)
{
    Com_Generic_c(ctx, argnum - 1);
}

/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState(void)
{
    S_StopAllSounds();
    OGG_Stop();
    SCR_StopCinematic();
    CL_ClearEffects();
    CL_ClearTEnts();
#ifdef AQTION_EXTENSION
	CL_Clear3DGhudQueue();
#endif
    LOC_FreeLocations();

    // wipe the entire cl structure
    BSP_Free(cl.bsp);
    memset(&cl, 0, sizeof(cl));
    memset(&cl_entities, 0, sizeof(cl_entities));


    if (cls.state > ca_connected) {
        cls.state = ca_connected;
        CL_CheckForPause();
        CL_UpdateFrameTimes();
    }

    // unprotect game cvar
    fs_game->flags &= ~CVAR_ROM;

#if USE_REF
    // unprotect our custom modulate cvars
    gl_modulate_world->flags &= ~CVAR_CHEAT;
    gl_modulate_entities->flags &= ~CVAR_CHEAT;
    gl_brightness->flags &= ~CVAR_CHEAT;
#endif
}

/*
=====================
CL_Disconnect

Goes from a connected state to full screen console state
Sends a disconnect message to the server
This is also called on Com_Error, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect(error_type_t type)
{
    if (!cls.state) {
        return;
    }

    SCR_EndLoadingPlaque(); // get rid of loading plaque

    SCR_ClearChatHUD_f();   // clear chat HUD on server change

    if (cls.state > ca_disconnected && !cls.demo.playback) {
        EXEC_TRIGGER(cl_disconnectcmd);
    }

    //cls.connect_time = 0;
    //cls.connect_count = 0;
    cls.passive = false;
#if USE_ICMP
    cls.errorReceived = false;
#endif

    if (cls.netchan.protocol) {
        // send a disconnect message to the server
        MSG_WriteByte(clc_stringcmd);
        MSG_WriteData("disconnect", 11);

        cls.netchan.Transmit(&cls.netchan, msg_write.cursize, msg_write.data, 3);

        SZ_Clear(&msg_write);

        Netchan_Close(&cls.netchan);
    }

    // stop playback and/or recording
    CL_CleanupDemos();

    // stop download
    CL_CleanupDownloads();

    CL_ClearState();

    CL_GTV_Suspend();

    cls.state = ca_disconnected;
    cls.userinfo_modified = 0;

    if (type == ERR_DISCONNECT) {
        UI_OpenMenu(UIMENU_DEFAULT);
    } else {
        UI_OpenMenu(UIMENU_NONE);
    }

    CL_CheckForPause();

    CL_UpdateFrameTimes();
}

/*
================
CL_Disconnect_f
================
*/
static void CL_Disconnect_f(void)
{
    if (cls.state > ca_disconnected) {
        Com_Error(ERR_DISCONNECT, "Disconnected from server");
    }
}

static void CL_ServerStatus_c(genctx_t *ctx, int argnum)
{
    if (argnum == 1) {
        CL_RecentIP_g(ctx);
        Com_Address_g(ctx);
    }
}

/*
================
CL_ServerStatus_f
================
*/
static void CL_ServerStatus_f(void)
{
    char        *s;
    netadr_t    adr;

    if (Cmd_Argc() < 2) {
        adr = cls.netchan.remote_address;
        if (!adr.type) {
            Com_Printf("Usage: %s [address]\n", Cmd_Argv(0));
            return;
        }
    } else {
        s = Cmd_Argv(1);
        if (!NET_StringToAdr(s, &adr, PORT_SERVER)) {
            Com_Printf("Bad address: %s\n", s);
            return;
        }
    }

    CL_AddRequest(&adr, REQ_STATUS_CL);

    NET_Config(NET_CLIENT);

    OOB_PRINT(NS_CLIENT, &adr, "status");
}

/*
====================
SortPlayers
====================
*/
static int SortPlayers(const void *v1, const void *v2)
{
    const playerStatus_t *p1 = (const playerStatus_t *)v1;
    const playerStatus_t *p2 = (const playerStatus_t *)v2;

    return p2->score - p1->score;
}

/*
====================
CL_ParseStatusResponse
====================
*/
static void CL_ParseStatusResponse(serverStatus_t *status, const char *string)
{
    playerStatus_t *player;
    const char *s;
    size_t infolen;

    // parse '\n' terminated infostring
    s = Q_strchrnul(string, '\n');

    // due to off-by-one error in the original version of Info_SetValueForKey,
    // some servers produce infostrings up to 512 characters long. work this
    // bug around by cutting off the last character(s).
    infolen = s - string;
    if (infolen >= MAX_INFO_STRING)
        infolen = MAX_INFO_STRING - 1;

    // copy infostring off
    memcpy(status->infostring, string, infolen);
    status->infostring[infolen] = 0;

    if (!Info_Validate(status->infostring))
        strcpy(status->infostring, "\\hostname\\badinfo");

    // parse optional player list
    status->numPlayers = 0;
    while (status->numPlayers < MAX_STATUS_PLAYERS) {
        player = &status->players[status->numPlayers];
        player->score = atoi(COM_Parse(&s));
        player->ping = atoi(COM_Parse(&s));
        Q_strlcpy(player->name, COM_Parse(&s), sizeof(player->name));
        if (!s)
            break;
        status->numPlayers++;
    }

    // sort players by frags
    qsort(status->players, status->numPlayers,
          sizeof(status->players[0]), SortPlayers);
}

static void CL_DumpStatusResponse(const serverStatus_t *status)
{
    int i;

    Com_Printf("Status response from %s\n\n", NET_AdrToString(&net_from));

    Info_Print(status->infostring);

    Com_Printf("\nNum Score Ping Name\n");
    for (i = 0; i < status->numPlayers; i++) {
        Com_Printf("%3i %5i %4i %s\n", i + 1,
                   status->players[i].score,
                   status->players[i].ping,
                   status->players[i].name);
    }
}

/*
====================
CL_ParsePrintMessage
====================
*/
static void CL_ParsePrintMessage(void)
{
    char string[MAX_NET_STRING];
    serverStatus_t status;
    request_t *r;

    MSG_ReadString(string, sizeof(string));

    r = CL_FindRequest();
    if (r) {
        switch (r->type) {
        case REQ_STATUS_CL:
            CL_ParseStatusResponse(&status, string);
            CL_DumpStatusResponse(&status);
            break;
#if USE_UI
        case REQ_STATUS_UI:
            CL_ParseStatusResponse(&status, string);
            UI_StatusEvent(&status);
            break;
#endif
#if USE_DISCORD && USE_CURL && USE_AQTION //rekkie -- discord  --s
        case REQ_STATUS_DISCORD:
            CL_DiscordParseServerStatus(&status, string);
            break;
#endif //rekkie -- discord -- e
        case REQ_RCON:
            Com_Printf("%s", string);
            return; // rcon may come in multiple packets

        default:
            return;
        }

        if (r->adr.type != NA_BROADCAST)
            r->type = REQ_FREE;
        return;
    }

    // finally, check is this is response from the server we are connecting to
    // and if so, start channenge cycle again
    if ((cls.state == ca_challenging || cls.state == ca_connecting) &&
        NET_IsEqualBaseAdr(&net_from, &cls.serverAddress)) {
        Com_Printf("%s", string);
        cls.state = ca_challenging;
        //cls.connect_count = 0;
        return;
    }

    Com_DPrintf("%s: dropped unrequested packet\n", __func__);
}

/*
=================
CL_ParseInfoMessage

Handle a reply from a ping
=================
*/
static void CL_ParseInfoMessage(void)
{
    char string[MAX_QPATH];
    request_t *r;

    r = CL_FindRequest();
    if (!r)
        return;
    if (r->type != REQ_INFO)
        return;

    MSG_ReadString(string, sizeof(string));
    Com_Printf("%s", string);
    if (r->adr.type != NA_BROADCAST)
        r->type = REQ_FREE;
}

/*
=================
CL_Changing_f

Just sent as a hint to the client that they should
drop to full console
=================
*/
static void CL_Changing_f(void)
{
    int i, j;
    char *s;

    if (cls.state < ca_connected) {
        return;
    }

    if (cls.demo.recording)
        CL_Stop_f();

    Com_Printf("Changing map...\n");

    if (!cls.demo.playback) {
        EXEC_TRIGGER(cl_changemapcmd);
        Cmd_ExecTrigger("#cl_changelevel");
    }

    SCR_BeginLoadingPlaque();

    cls.state = ca_connected;   // not active anymore, but not disconnected
    cl.mapname[0] = 0;
    cl.configstrings[CS_NAME][0] = 0;

    CL_CheckForPause();

    CL_UpdateFrameTimes();

    // parse additional parameters
    j = Cmd_Argc();
    for (i = 1; i < j; i++) {
        s = Cmd_Argv(i);
        if (!strncmp(s, "map=", 4)) {
            Q_strlcpy(cl.mapname, s + 4, sizeof(cl.mapname));
        }
    }

    SCR_UpdateScreen();
}


/*
=================
CL_Reconnect_f

The server is changing levels
=================
*/
static void CL_Reconnect_f(void)
{
    if (cls.demo.playback) {
        Com_Printf("No server to reconnect to.\n");
        return;
    }

    if (cls.state >= ca_precached || Cmd_From() != FROM_STUFFTEXT) {
        CL_Disconnect(ERR_RECONNECT);
    }

    if (cls.state >= ca_connected) {
        cls.state = ca_connected;

        if (cls.download.file) {
            return; // if we are downloading, we don't change!
        }

        Com_Printf("Reconnecting...\n");

        CL_ClientCommand("new");
        return;
    }

    // issued manually at console
    if (cls.serverAddress.type == NA_UNSPECIFIED) {
        Com_Printf("No server to reconnect to.\n");
        return;
    }
    if (cls.serverAddress.type == NA_LOOPBACK && !sv_running->integer) {
        Com_Printf("Can not reconnect to loopback.\n");
        return;
    }

    Com_Printf("Reconnecting...\n");

    cls.serverProtocol = cl_protocol->integer;
    cls.state = ca_challenging;
    cls.connect_time -= CONNECT_FAST;
    cls.connect_count = 0;

    SCR_UpdateScreen();
}

#if USE_UI
/*
=================
CL_SendStatusRequest
=================
*/
void CL_SendStatusRequest(const netadr_t *address)
{
    NET_Config(NET_CLIENT);

    CL_AddRequest(address, REQ_STATUS_UI);

    OOB_PRINT(NS_CLIENT, address, "status");
}
#endif

/*
=================
CL_PingServers_f
=================
*/
static void CL_PingServers_f(void)
{
    netadr_t address;
    cvar_t *var;
    int i;

    NET_Config(NET_CLIENT);

    // send a broadcast packet
    memset(&address, 0, sizeof(address));
    address.type = NA_BROADCAST;
    address.port = BigShort(PORT_SERVER);

    Com_DPrintf("Pinging broadcast...\n");
    CL_AddRequest(&address, REQ_INFO);

    OOB_PRINT(NS_CLIENT, &address, "info 34");

    // send a packet to each address book entry
    for (i = 0; i < 64; i++) {
        var = Cvar_FindVar(va("adr%i", i));
        if (!var)
            break;

        if (!var->string[0])
            continue;

        if (!NET_StringToAdr(var->string, &address, PORT_SERVER)) {
            Com_Printf("Bad address: %s\n", var->string);
            continue;
        }

        Com_DPrintf("Pinging %s...\n", var->string);
        CL_AddRequest(&address, REQ_INFO);

        OOB_PRINT(NS_CLIENT, &address, "info 34");
    }
}

/*
=================
CL_Skins_f

Load or download any custom player skins and models
=================
*/
static void CL_Skins_f(void)
{
    int i;
    char *s;
    clientinfo_t *ci;

    if (cls.state < ca_precached) {
        Com_Printf("Must be in a level to load skins.\n");
        return;
    }

    CL_RegisterVWepModels();

    for (i = 0; i < MAX_CLIENTS; i++) {
        s = cl.configstrings[CS_PLAYERSKINS + i];
        if (!s[0])
            continue;
        ci = &cl.clientinfo[i];
        CL_LoadClientinfo(ci, s);
        if (!ci->model_name[0] || !ci->skin_name[0])
            ci = &cl.baseclientinfo;
        Com_Printf("client %d: %s --> %s/%s\n", i, s,
                   ci->model_name, ci->skin_name);
        SCR_UpdateScreen();
    }
}

static void cl_noskins_changed(cvar_t *self)
{
    int i;
    char *s;
    clientinfo_t *ci;

    if (cls.state < ca_precached) {
        return;
    }

    for (i = 0; i < MAX_CLIENTS; i++) {
        s = cl.configstrings[CS_PLAYERSKINS + i];
        if (!s[0])
            continue;
        ci = &cl.clientinfo[i];
        CL_LoadClientinfo(ci, s);
    }
}

static void cl_vwep_changed(cvar_t *self)
{
    if (cls.state < ca_precached) {
        return;
    }

    CL_RegisterVWepModels();
    cl_noskins_changed(self);
}

static void CL_Name_g(genctx_t *ctx)
{
    int i;
    char buffer[MAX_CLIENT_NAME];

    if (cls.state < ca_precached) {
        return;
    }

    for (i = 0; i < MAX_CLIENTS; i++) {
        Q_strlcpy(buffer, cl.clientinfo[i].name, sizeof(buffer));
        if (COM_strclr(buffer))
            Prompt_AddMatch(ctx, buffer);
    }
}


/*
=================
CL_ConnectionlessPacket

Responses to broadcasts, etc
=================
*/
static void CL_ConnectionlessPacket(void)
{
    char    string[MAX_STRING_CHARS];
    char    *s, *c;
    int     i, j, k;

    MSG_BeginReading();
    MSG_ReadLong(); // skip the -1

    if (MSG_ReadStringLine(string, sizeof(string)) >= sizeof(string)) {
        Com_DPrintf("Oversize message received.  Ignored.\n");
        return;
    }

    Cmd_TokenizeString(string, false);

    c = Cmd_Argv(0);

    Com_DPrintf("%s: %s\n", NET_AdrToString(&net_from), string);

    // challenge from the server we are connecting to
    if (!strcmp(c, "challenge")) {
        int mask = 0;

        if (cls.state < ca_challenging) {
            Com_DPrintf("Challenge received while not connecting.  Ignored.\n");
            return;
        }
        if (!NET_IsEqualBaseAdr(&net_from, &cls.serverAddress)) {
            Com_DPrintf("Challenge from different address.  Ignored.\n");
            return;
        }
        if (cls.state > ca_challenging) {
            Com_DPrintf("Dup challenge received.  Ignored.\n");
            return;
        }

        cls.challenge = atoi(Cmd_Argv(1));
        cls.state = ca_connecting;
        cls.connect_time -= CONNECT_INSTANT; // fire immediately
        //cls.connect_count = 0;

        // parse additional parameters
        j = Cmd_Argc();
        for (i = 2; i < j; i++) {
            s = Cmd_Argv(i);
            if (!strncmp(s, "p=", 2)) {
                s += 2;
                while (*s) {
                    k = strtoul(s, &s, 10);
                    if (k == PROTOCOL_VERSION_R1Q2) {
                        mask |= 1;
                    } else if (k == PROTOCOL_VERSION_Q2PRO) {
                        mask |= 2;
                    } else if (k == PROTOCOL_VERSION_AQTION) {
						mask |= 4;
					}
                    s = strchr(s, ',');
                    if (s == NULL) {
                        break;
                    }
                    s++;
                }
            }
        }

        if (!cls.serverProtocol) {
            cls.serverProtocol = PROTOCOL_VERSION_Q2PRO;
        }

        // choose supported protocol
        switch (cls.serverProtocol) {
		case PROTOCOL_VERSION_AQTION:
			if (mask & 4) {
				break;
			}
			cls.serverProtocol = PROTOCOL_VERSION_Q2PRO;
			// fall through
        case PROTOCOL_VERSION_Q2PRO:
            if (mask & 2) {
                break;
            }
            cls.serverProtocol = PROTOCOL_VERSION_R1Q2;
            // fall through
        case PROTOCOL_VERSION_R1Q2:
            if (mask & 1) {
                break;
            }
            // fall through
        default:
            cls.serverProtocol = PROTOCOL_VERSION_DEFAULT;
            break;
        }
        Com_DPrintf("Selected protocol %d\n", cls.serverProtocol);

        CL_CheckForResend();
        return;
    }

    // server connection
    if (!strcmp(c, "client_connect")) {
        netchan_type_t type;
        int anticheat = 0;
        char mapname[MAX_QPATH];
        bool got_server = false;

        if (cls.state < ca_connecting) {
            Com_DPrintf("Connect received while not connecting.  Ignored.\n");
            return;
        }
        if (!NET_IsEqualBaseAdr(&net_from, &cls.serverAddress)) {
            Com_DPrintf("Connect from different address.  Ignored.\n");
            return;
        }
        if (cls.state > ca_connecting) {
            Com_DPrintf("Dup connect received.  Ignored.\n");
            return;
        }

        if (cls.serverProtocol == PROTOCOL_VERSION_Q2PRO || cls.serverProtocol == PROTOCOL_VERSION_AQTION) {
            type = NETCHAN_NEW;
        } else {
            type = NETCHAN_OLD;
        }

        mapname[0] = 0;

        // parse additional parameters
        j = Cmd_Argc();
        for (i = 1; i < j; i++) {
            s = Cmd_Argv(i);
            if (!strncmp(s, "ac=", 3)) {
                s += 3;
                if (*s) {
                    anticheat = atoi(s);
                }
            } else if (!strncmp(s, "nc=", 3)) {
                s += 3;
                if (*s) {
                    type = atoi(s);
                    if (type != NETCHAN_OLD && type != NETCHAN_NEW) {
                        Com_Error(ERR_DISCONNECT,
                                  "Server returned invalid netchan type");
                    }
                }
            } else if (!strncmp(s, "map=", 4)) {
                Q_strlcpy(mapname, s + 4, sizeof(mapname));
            } else if (!strncmp(s, "dlserver=", 9)) {
                if (!got_server) {
                    HTTP_SetServer(s + 9);
                    got_server = true;
                }
            }
        }

        if (!got_server) {
            HTTP_SetServer(NULL);
        }

        Com_Printf("Connected to %s (protocol %d).\n",
                   NET_AdrToString(&cls.serverAddress), cls.serverProtocol);
        Netchan_Close(&cls.netchan);
        Netchan_Setup(&cls.netchan, NS_CLIENT, type, &cls.serverAddress,
                      cls.quakePort, 1024, cls.serverProtocol);

#if USE_AC_CLIENT
        if (anticheat) {
            MSG_WriteByte(clc_nop);
            MSG_FlushTo(&cls.netchan.message);
            cls.netchan.Transmit(&cls.netchan, 0, "", 3);
            S_StopAllSounds();
            cls.connect_count = -1;
            Com_Printf("Loading anticheat, this may take a few moments...\n");
            SCR_UpdateScreen();
            if (!Sys_GetAntiCheatAPI()) {
                Com_Printf("Trying to connect without anticheat.\n");
            } else {
                Com_LPrintf(PRINT_NOTICE, "Anticheat loaded successfully.\n");
            }
        }
#else
        if (anticheat >= 2) {
            Com_Printf("Anticheat required by server, "
                       "but no anticheat support linked in.\n");
        }
#endif

        CL_ClientCommand("new");
        cls.state = ca_connected;
        cls.connect_count = 0;
        Q_strlcpy(cl.mapname, mapname, sizeof(cl.mapname)); // for levelshot screen
        return;
    }

    if (!strcmp(c, "passive_connect")) {
        if (!cls.passive) {
            Com_DPrintf("Passive connect received while not connecting.  Ignored.\n");
            return;
        }
        s = NET_AdrToString(&net_from);
        Com_Printf("Received passive connect from %s.\n", s);

        cls.serverAddress = net_from;
        cls.serverProtocol = cl_protocol->integer;
        Q_strlcpy(cls.servername, s, sizeof(cls.servername));
        cls.passive = false;

        cls.state = ca_challenging;
        cls.connect_time -= CONNECT_FAST;
        cls.connect_count = 0;

        CL_CheckForResend();
        return;
    }

    // print command from somewhere
    if (!strcmp(c, "print")) {
        CL_ParsePrintMessage();
        return;
    }

    // server responding to a status broadcast
    if (!strcmp(c, "info")) {
        CL_ParseInfoMessage();
        return;
    }

    Com_DPrintf("Unknown connectionless packet command.\n");
}

/*
=================
CL_PacketEvent
=================
*/
static void CL_PacketEvent(void)
{
    if (msg_read.cursize < 4) {
        return;
    }

    //
    // remote command packet
    //
    if (*(int *)msg_read.data == -1) {
        CL_ConnectionlessPacket();
        return;
    }

    if (cls.state < ca_connected) {
        return;
    }

    if (cls.demo.playback) {
        return;     // dump it if not connected
    }

    if (msg_read.cursize < 8) {
        Com_DPrintf("%s: runt packet\n", NET_AdrToString(&net_from));
        return;
    }

    //
    // packet from server
    //
    if (!NET_IsEqualAdr(&net_from, &cls.netchan.remote_address)) {
        Com_DPrintf("%s: sequenced packet without connection\n",
                    NET_AdrToString(&net_from));
        return;
    }

    if (!cls.netchan.Process(&cls.netchan))
        return;     // wasn't accepted for some reason

#if USE_ICMP
    cls.errorReceived = false; // don't drop
#endif

    CL_ParseServerMessage();

    SCR_LagSample();

    // if recording demo, write the message out
    if (cls.demo.recording && !cls.demo.paused && CL_FRAMESYNC) {
        CL_WriteDemoMessage(&cls.demo.buffer);
    }

    // if running GTV server, transmit to client
    CL_GTV_Transmit();
}

#if USE_ICMP
void CL_ErrorEvent(netadr_t *from)
{
    UI_ErrorEvent(from);

    //
    // error packet from server
    //
    if (cls.state < ca_connected) {
        return;
    }
    if (cls.demo.playback) {
        return;     // dump it if not connected
    }
    if (!NET_IsEqualBaseAdr(from, &cls.netchan.remote_address)) {
        return;
    }
    if (from->port && from->port != cls.netchan.remote_address.port) {
        return;
    }

    cls.errorReceived = true; // drop connection soon
}
#endif


//=============================================================================

/*
==============
CL_FixUpGender_f
==============
*/
static void CL_FixUpGender(void)
{
    char *p;
    char sk[MAX_QPATH];

    Q_strlcpy(sk, info_skin->string, sizeof(sk));
    if ((p = strchr(sk, '/')) != NULL)
        *p = 0;
    if (Q_stricmp(sk, "male") == 0 || Q_stricmp(sk, "cyborg") == 0)
        Cvar_Set("gender", "male");
    else if (Q_stricmp(sk, "female") == 0 || Q_stricmp(sk, "crackhor") == 0)
        Cvar_Set("gender", "female");
    else
        Cvar_Set("gender", "none");
    info_gender->modified = false;
}

void CL_UpdateUserinfo(cvar_t *var, from_t from)
{
    int i;

    if (var == info_skin && from > FROM_CONSOLE && gender_auto->integer) {
        CL_FixUpGender();
    }

    if (cls.state < ca_connected) {
        return;
    }

    if (cls.demo.playback) {
        return;
    }

    if (var->flags & CVAR_PRIVATE) {
        return;
    }

    if (cls.serverProtocol != PROTOCOL_VERSION_Q2PRO && cls.serverProtocol != PROTOCOL_VERSION_AQTION) {
        // transmit at next oportunity
        cls.userinfo_modified = MAX_PACKET_USERINFOS;
        goto done;
    }

    if (cls.userinfo_modified == MAX_PACKET_USERINFOS) {
        // can't hold any more
        goto done;
    }

    // check for the same variable being modified twice
    for (i = 0; i < cls.userinfo_modified; i++) {
        if (cls.userinfo_updates[i] == var) {
            Com_DDPrintf("%s: %u: %s [DUP]\n",
                         __func__, com_framenum, var->name);
            return;
        }
    }

    cls.userinfo_updates[cls.userinfo_modified++] = var;

done:
    Com_DDPrintf("%s: %u: %s [%d]\n",
                 __func__, com_framenum, var->name, cls.userinfo_modified);
}

/*
==============
CL_Userinfo_f
==============
*/
static void CL_Userinfo_f(void)
{
    char userinfo[MAX_INFO_STRING];

    Cvar_BitInfo(userinfo, CVAR_USERINFO);

    Com_Printf("User info settings:\n");
    Info_Print(userinfo);
}

/*
=================
CL_RestartSound_f

Restart the sound subsystem so it can pick up
new parameters and flush all sounds
=================
*/
static void CL_RestartSound_f(void)
{
    S_Shutdown();
    S_Init();
    CL_RegisterSounds();
}

/*
=================
CL_PlaySound_f

Moved here from sound code so that command is always registered.
=================
*/
static void CL_PlaySound_c(genctx_t *ctx, int state)
{
    FS_File_g("sound", "*.wav", FS_SEARCH_SAVEPATH | FS_SEARCH_BYFILTER | FS_SEARCH_STRIPEXT, ctx);
}

static void CL_PlaySound_f(void)
{
    int     i;
    char name[MAX_QPATH];

    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: %s <sound> [...]\n", Cmd_Argv(0));
        return;
    }

    for (i = 1; i < Cmd_Argc(); i++) {
        Cmd_ArgvBuffer(i, name, sizeof(name));
        COM_DefaultExtension(name, ".wav", sizeof(name));
        S_StartLocalSound(name);
    }
}

static int precache_spawncount;

/*
=================
CL_Begin

Called after all downloads are done. Not used for demos.
=================
*/
void CL_Begin(void)
{
#if USE_REF
    if (!Q_stricmp(cl.gamedir, "gloom")) {
        // cheat protect our custom modulate cvars
        gl_modulate_world->flags |= CVAR_CHEAT;
        gl_modulate_entities->flags |= CVAR_CHEAT;
        gl_brightness->flags |= CVAR_CHEAT;
    }
#endif

    Cvar_FixCheats();

    CL_PrepRefresh();
    CL_LoadState(LOAD_SOUNDS);
    CL_RegisterSounds();
    LOC_LoadLocations();
    CL_LoadState(LOAD_NONE);
    cls.state = ca_precached;

#if USE_FPS
    CL_UpdateRateSetting();
#endif

    CL_ClientCommand(va("begin %i\n", precache_spawncount));

    CL_UpdateGunSetting();
    CL_UpdateBlendSetting();
    CL_UpdateGibSetting();
    CL_UpdateFootstepsSetting();
    CL_UpdatePredictSetting();
    CL_UpdateRecordingSetting();
}

/*
=================
CL_Precache_f

The server will send this command right
before allowing the client into the server
=================
*/
static void CL_Precache_f(void)
{
    if (cls.state < ca_connected) {
        return;
    }

    cls.state = ca_loading;
    CL_LoadState(LOAD_MAP);

    S_StopAllSounds();

    CL_RegisterVWepModels();

    // demos use different precache sequence
    if (cls.demo.playback) {
        CL_RegisterBspModels();
        CL_PrepRefresh();
        CL_LoadState(LOAD_SOUNDS);
        CL_RegisterSounds();
        CL_LoadState(LOAD_NONE);
        cls.state = ca_precached;
        return;
    }

    precache_spawncount = atoi(Cmd_Argv(1));

    CL_ResetPrecacheCheck();
    CL_RequestNextDownload();

    if (cls.state != ca_precached) {
        cls.state = ca_connected;
    }
}

void CL_LoadFilterList(string_entry_t **list, const char *name, const char *comments, size_t maxlen)
{
    string_entry_t *entry, *next;
    char *raw, *data, *p;
    int len, count, line;

    // free previous entries
    for (entry = *list; entry; entry = next) {
        next = entry->next;
        Z_Free(entry);
    }

    *list = NULL;

    // load new list
    len = FS_LoadFileEx(name, (void **)&raw, FS_TYPE_REAL, TAG_FILESYSTEM);
    if (!raw) {
        if (len != Q_ERR(ENOENT))
            Com_EPrintf("Couldn't load %s: %s\n", name, Q_ErrorString(len));
        return;
    }

    count = 0;
    line = 1;
    data = raw;

    while (*data) {
        p = strchr(data, '\n');
        if (p) {
            if (p > data && *(p - 1) == '\r')
                *(p - 1) = 0;
            *p = 0;
        }

        // ignore empty lines and comments
        if (*data && (!comments || !strchr(comments, *data))) {
            len = strlen(data);
            if (len < maxlen) {
                entry = Z_Malloc(sizeof(*entry) + len);
                memcpy(entry->string, data, len + 1);
                entry->next = *list;
                *list = entry;
                count++;
            } else {
                Com_WPrintf("Oversize filter on line %d in %s\n", line, name);
            }
        }

        if (!p)
            break;

        data = p + 1;
        line++;
    }

    Com_DPrintf("Loaded %d filters from %s\n", count, name);

    FS_FreeFile(raw);
}

static void CL_LoadStuffTextWhiteList(void)
{
    CL_LoadFilterList(&cls.stufftextwhitelist, "stufftext-whitelist.txt", NULL, MAX_STRING_CHARS);
}

typedef struct {
    list_t entry;
    unsigned hits;
    char match[1];
} ignore_t;

static list_t   cl_ignore_text;
static list_t   cl_ignore_nick;

static ignore_t *find_ignore(list_t *list, const char *match)
{
    ignore_t *ignore;

    LIST_FOR_EACH(ignore_t, ignore, list, entry) {
        if (!strcmp(ignore->match, match)) {
            return ignore;
        }
    }

    return NULL;
}

static void list_ignores(list_t *list)
{
    ignore_t *ignore;

    if (LIST_EMPTY(list)) {
        Com_Printf("No ignore filters.\n");
        return;
    }

    Com_Printf("Current ignore filters:\n");
    LIST_FOR_EACH(ignore_t, ignore, list, entry) {
        Com_Printf("\"%s\" (%u hit%s)\n", ignore->match,
                   ignore->hits, ignore->hits == 1 ? "" : "s");
    }
}

static void add_ignore(list_t *list, const char *match, size_t minlen)
{
    ignore_t *ignore;
    size_t matchlen;

    // don't create the same ignore twice
    if (find_ignore(list, match)) {
        return;
    }

    matchlen = strlen(match);
    if (matchlen < minlen) {
        Com_Printf("Match string \"%s\" is too short.\n", match);
        return;
    }

    ignore = Z_Malloc(sizeof(*ignore) + matchlen);
    ignore->hits = 0;
    memcpy(ignore->match, match, matchlen + 1);
    List_Append(list, &ignore->entry);
}

static void remove_ignore(list_t *list, const char *match)
{
    ignore_t *ignore;

    ignore = find_ignore(list, match);
    if (!ignore) {
        Com_Printf("Can't find ignore filter \"%s\"\n", match);
        return;
    }

    List_Remove(&ignore->entry);
    Z_Free(ignore);
}

static void remove_all_ignores(list_t *list)
{
    ignore_t *ignore, *next;
    int count = 0;

    LIST_FOR_EACH_SAFE(ignore_t, ignore, next, list, entry) {
        Z_Free(ignore);
        count++;
    }

    Com_Printf("Removed %d ignore filter%s.\n", count, count == 1 ? "" : "s");
    List_Init(list);
}

static void CL_IgnoreText_f(void)
{
    if (Cmd_Argc() == 1) {
        list_ignores(&cl_ignore_text);
        return;
    }

    add_ignore(&cl_ignore_text, Cmd_ArgsFrom(1), 3);
}

static void CL_UnIgnoreText_f(void)
{
    if (Cmd_Argc() == 1) {
        list_ignores(&cl_ignore_text);
        return;
    }

    if (Cmd_Argc() == 2 && !strcmp(Cmd_Argv(1), "all")) {
        remove_all_ignores(&cl_ignore_text);
        return;
    }

    remove_ignore(&cl_ignore_text, Cmd_ArgsFrom(1));
}

static void CL_IgnoreNick_c(genctx_t *ctx, int argnum)
{
    if (argnum == 1) {
        CL_Name_g(ctx);
    }
}

static void CL_UnIgnoreNick_c(genctx_t *ctx, int argnum)
{
    ignore_t *ignore;

    if (argnum == 1) {
        LIST_FOR_EACH(ignore_t, ignore, &cl_ignore_nick, entry) {
            Prompt_AddMatch(ctx, ignore->match);
        }
    }
}

static void CL_IgnoreNick_f(void)
{
    if (Cmd_Argc() == 1) {
        list_ignores(&cl_ignore_nick);
        return;
    }

    add_ignore(&cl_ignore_nick, Cmd_Argv(1), 1);
}

static void CL_UnIgnoreNick_f(void)
{
    if (Cmd_Argc() == 1) {
        list_ignores(&cl_ignore_nick);
        return;
    }

    if (Cmd_Argc() == 2 && !strcmp(Cmd_Argv(1), "all")) {
        remove_all_ignores(&cl_ignore_nick);
        return;
    }

    remove_ignore(&cl_ignore_nick, Cmd_Argv(1));
}

static bool match_ignore_nick_2(const char *nick, const char *s)
{
    size_t len = strlen(nick);

    if (!strncmp(s, nick, len) && !strncmp(s + len, ": ", 2))
        return true;

    if (*s == '(') {
        s++;
        return !strncmp(s, nick, len) && !strncmp(s + len, "): ", 3);
    }

    return false;
}

static bool match_ignore_nick(const char *nick, const char *s)
{
    if (match_ignore_nick_2(nick, s))
        return true;

    if (*s == '[') {
        char *p = strstr(s + 1, "] ");
        if (p)
            return match_ignore_nick_2(nick, p + 2);
    }

    return false;
}

/*
=================
CL_CheckForIgnore
=================
*/
bool CL_CheckForIgnore(const char *s)
{
    char buffer[MAX_STRING_CHARS];
    ignore_t *ignore;

    if (LIST_EMPTY(&cl_ignore_text) && LIST_EMPTY(&cl_ignore_nick)) {
        return false;
    }

    Q_strlcpy(buffer, s, sizeof(buffer));
    COM_strclr(buffer);

    LIST_FOR_EACH(ignore_t, ignore, &cl_ignore_text, entry) {
        if (Com_WildCmp(ignore->match, buffer)) {
            ignore->hits++;
            return true;
        }
    }

    LIST_FOR_EACH(ignore_t, ignore, &cl_ignore_nick, entry) {
        if (match_ignore_nick(ignore->match, buffer)) {
            ignore->hits++;
            return true;
        }
    }

    return false;
}

static void CL_DumpClients_f(void)
{
    int i;

    if (cls.state != ca_active) {
        Com_Printf("Must be in a level to dump.\n");
        return;
    }

    for (i = 0; i < MAX_CLIENTS; i++) {
        if (!cl.clientinfo[i].name[0]) {
            continue;
        }

        Com_Printf("%3i: %s\n", i, cl.clientinfo[i].name);
    }
}

static void dump_program(const char *text, const char *name)
{
    char buffer[MAX_OSPATH];

    if (cls.state != ca_active) {
        Com_Printf("Must be in a level to dump.\n");
        return;
    }

    if (Cmd_Argc() != 2) {
        Com_Printf("Usage: %s <filename>\n", Cmd_Argv(0));
        return;
    }

    if (!*text) {
        Com_Printf("No %s to dump.\n", name);
        return;
    }

    if (FS_EasyWriteFile(buffer, sizeof(buffer), FS_MODE_WRITE | FS_FLAG_TEXT,
                         "layouts/", Cmd_Argv(1), ".txt", text, strlen(text))) {
        Com_Printf("Dumped %s program to %s.\n", name, buffer);
    }
}

static void CL_DumpStatusbar_f(void)
{
    dump_program(cl.configstrings[CS_STATUSBAR], "status bar");
}

static void CL_DumpLayout_f(void)
{
    dump_program(cl.layout, "layout");
}

static const cmd_option_t o_writeconfig[] = {
    { "a", "aliases", "write aliases" },
    { "b", "bindings", "write bindings" },
    { "c", "cvars", "write archived cvars" },
    { "h", "help", "display this help message" },
    { "m", "modified", "write modified cvars" },
    { NULL }
};

static void CL_WriteConfig_c(genctx_t *ctx, int argnum)
{
    Cmd_Option_c(o_writeconfig, Cmd_Config_g, ctx, argnum);
}

/*
===============
CL_WriteConfig_f
===============
*/
static void CL_WriteConfig_f(void)
{
    char buffer[MAX_OSPATH];
    bool aliases = false, bindings = false, modified = false;
    int c, mask = 0;
    qhandle_t f;

    while ((c = Cmd_ParseOptions(o_writeconfig)) != -1) {
        switch (c) {
        case 'a':
            aliases = true;
            break;
        case 'b':
            bindings = true;
            break;
        case 'c':
            mask |= CVAR_ARCHIVE;
            break;
        case 'h':
            Cmd_PrintUsage(o_writeconfig, "<filename>");
            Com_Printf("Save current configuration into file.\n");
            Cmd_PrintHelp(o_writeconfig);
            return;
        case 'm':
            modified = true;
            mask = ~0;
            break;
        default:
            return;
        }
    }

    if (!cmd_optarg[0]) {
        Com_Printf("Missing filename argument.\n");
        Cmd_PrintHint();
        return;
    }

    if (!aliases && !bindings && !mask) {
        bindings = true;
        mask = CVAR_ARCHIVE;
    }

    f = FS_EasyOpenFile(buffer, sizeof(buffer), FS_MODE_WRITE | FS_FLAG_TEXT,
                        "configs/", cmd_optarg, ".cfg");
    if (!f) {
        return;
    }

    FS_FPrintf(f, "// generated by q2pro\n");

    if (bindings) {
        FS_FPrintf(f, "\n// key bindings\n");
        Key_WriteBindings(f);
    }
    if (aliases) {
        FS_FPrintf(f, "\n// command aliases\n");
        Cmd_WriteAliases(f);
    }
    if (mask) {
        FS_FPrintf(f, "\n//%s cvars\n", modified ? "modified" : "archived");
        Cvar_WriteVariables(f, mask, modified);
    }

    if (FS_CloseFile(f))
        Com_EPrintf("Error writing %s\n", buffer);
    else
        Com_Printf("Wrote %s.\n", buffer);
}

static void CL_Say_c(genctx_t *ctx, int argnum)
{
    CL_Name_g(ctx);
}

static size_t CL_Mapname_m(char *buffer, size_t size)
{
    return Q_strlcpy(buffer, cl.mapname, size);
}

static size_t CL_Server_m(char *buffer, size_t size)
{
    return Q_strlcpy(buffer, cls.servername, size);
}

static size_t CL_Ups_m(char *buffer, size_t size)
{
    vec3_t vel;

    if (!cls.demo.playback && cl_predict->integer &&
        !(cl.frame.ps.pmove.pm_flags & PMF_NO_PREDICTION)) {
        VectorCopy(cl.predicted_velocity, vel);
    } else {
        VectorScale(cl.frame.ps.pmove.velocity, 0.125f, vel);
    }

    return Q_scnprintf(buffer, size, "%.f", VectorLength(vel));
}

static size_t CL_Timer_m(char *buffer, size_t size)
{
    int hour, min, sec;

    sec = cl.time / 1000;
    min = sec / 60; sec %= 60;
    hour = min / 60; min %= 60;

    if (hour) {
        return Q_scnprintf(buffer, size, "%i:%i:%02i", hour, min, sec);
    }
    return Q_scnprintf(buffer, size, "%i:%02i", min, sec);
}

static size_t CL_DemoPos_m(char *buffer, size_t size)
{
    int sec, min, framenum;

    if (cls.demo.playback)
        framenum = cls.demo.frames_read;
    else if (!MVD_GetDemoStatus(NULL, NULL, &framenum))
        framenum = 0;

    sec = framenum / 10; framenum %= 10;
    min = sec / 60; sec %= 60;

    return Q_scnprintf(buffer, size, "%d:%02d.%d", min, sec, framenum);
}

static size_t CL_Fps_m(char *buffer, size_t size)
{
    return Q_scnprintf(buffer, size, "%i", C_FPS);
}

static size_t R_Fps_m(char *buffer, size_t size)
{
    return Q_scnprintf(buffer, size, "%i", R_FPS);
}

static size_t CL_Mps_m(char *buffer, size_t size)
{
    return Q_scnprintf(buffer, size, "%i", C_MPS);
}

static size_t CL_Pps_m(char *buffer, size_t size)
{
    return Q_scnprintf(buffer, size, "%i", C_PPS);
}

static size_t CL_Ping_m(char *buffer, size_t size)
{
    return Q_scnprintf(buffer, size, "%i", cls.measure.ping);
}

static size_t CL_Lag_m(char *buffer, size_t size)
{
    float f = 0.0f;

    if (cls.netchan.total_received)
        f = (float)cls.netchan.total_dropped / cls.netchan.total_received;

    return Q_scnprintf(buffer, size, "%.2f%%", f * 100.0f);
}

static size_t CL_Health_m(char *buffer, size_t size)
{
    return Q_scnprintf(buffer, size, "%i", cl.frame.ps.stats[STAT_HEALTH]);
}

static size_t CL_Ammo_m(char *buffer, size_t size)
{
    return Q_scnprintf(buffer, size, "%i", cl.frame.ps.stats[STAT_AMMO]);
}

static size_t CL_Armor_m(char *buffer, size_t size)
{
    return Q_scnprintf(buffer, size, "%i", cl.frame.ps.stats[STAT_ARMOR]);
}

static size_t CL_WeaponModel_m(char *buffer, size_t size)
{
    return Q_strlcpy(buffer, cl.configstrings[CS_MODELS + cl.frame.ps.gunindex], size);
}

/*
===============
CL_WriteConfig

Writes key bindings and archived cvars to config.cfg
===============
*/
static void CL_WriteConfig(void)
{
    qhandle_t f;
    int ret;

    ret = FS_OpenFile(COM_CONFIG_CFG, &f, FS_MODE_WRITE | FS_FLAG_TEXT);
    if (!f) {
        Com_EPrintf("Couldn't open %s for writing: %s\n",
                    COM_CONFIG_CFG, Q_ErrorString(ret));
        return;
    }

    FS_FPrintf(f, "// generated by " APPLICATION ", do not modify\n");

    Key_WriteBindings(f);
    Cvar_WriteVariables(f, CVAR_ARCHIVE, false);

    if (FS_CloseFile(f))
        Com_EPrintf("Error writing %s\n", COM_CONFIG_CFG);
}

/*
====================
CL_RestartFilesystem

Flush caches and restart the VFS.
====================
*/
void CL_RestartFilesystem(bool total)
{
    int cls_state;

    if (!cl_running->integer) {
        FS_Restart(total);
        return;
    }

    Com_DPrintf("%s(%d)\n", __func__, total);

    // temporary switch to loading state
    cls_state = cls.state;
    if (cls.state >= ca_precached) {
        cls.state = ca_loading;
    }

    Con_Popup(false);

    UI_Shutdown();

    S_StopAllSounds();
    S_FreeAllSounds();

    // write current config before changing game directory
    CL_WriteConfig();

    if (cls.ref_initialized) {
        R_Shutdown(false);

        FS_Restart(total);

        R_Init(false);

        SCR_RegisterMedia();
        Con_RegisterMedia();
        UI_Init();
    } else {
        FS_Restart(total);
    }

    if (cls_state == ca_disconnected) {
        UI_OpenMenu(UIMENU_DEFAULT);
    } else if (cls_state >= ca_loading && cls_state <= ca_active) {
        CL_LoadState(LOAD_MAP);
        CL_PrepRefresh();
        CL_LoadState(LOAD_SOUNDS);
        CL_RegisterSounds();
        CL_LoadState(LOAD_NONE);
    } else if (cls_state == ca_cinematic) {
        SCR_ReloadCinematic();
    }

    CL_LoadDownloadIgnores();
    CL_LoadStuffTextWhiteList();
    OGG_LoadTrackList();

    // switch back to original state
    cls.state = cls_state;

    Con_Close(false);

    CL_UpdateFrameTimes();

    cvar_modified &= ~CVAR_FILES;
}

void CL_RestartRefresh(bool total)
{
    int cls_state;

    if (!cls.ref_initialized) {
        return;
    }

    // temporary switch to loading state
    cls_state = cls.state;
    if (cls.state >= ca_precached) {
        cls.state = ca_loading;
    }

    Con_Popup(false);

    S_StopAllSounds();

    if (total) {
        IN_Shutdown();
        CL_ShutdownRefresh();
        CL_InitRefresh();
        IN_Init();
    } else {
        UI_Shutdown();
        R_Shutdown(false);
        R_Init(false);
        SCR_RegisterMedia();
        Con_RegisterMedia();
        UI_Init();
    }

    if (cls_state == ca_disconnected) {
        UI_OpenMenu(UIMENU_DEFAULT);
    } else if (cls_state >= ca_loading && cls_state <= ca_active) {
        CL_LoadState(LOAD_MAP);
        CL_PrepRefresh();
        CL_LoadState(LOAD_NONE);
    } else if (cls_state == ca_cinematic) {
        SCR_ReloadCinematic();
    }

    // switch back to original state
    cls.state = cls_state;

    Con_Close(false);

    CL_UpdateFrameTimes();

    cvar_modified &= ~CVAR_FILES;
}

/*
====================
CL_ReloadRefresh

Flush caches and reload all models and textures.
====================
*/
static void CL_ReloadRefresh_f(void)
{
    CL_RestartRefresh(false);
}

/*
====================
CL_RestartRefresh

Perform complete restart of the renderer subsystem.
====================
*/
static void CL_RestartRefresh_f(void)
{
    CL_RestartRefresh(true);
}

static bool allow_stufftext(const char *text)
{
    string_entry_t *entry;

    for (entry = cls.stufftextwhitelist; entry; entry = entry->next)
        if (Com_WildCmp(entry->string, text))
            return true;

    return false;
}

// execute string in server command buffer
static void exec_server_string(cmdbuf_t *buf, const char *text)
{
    char *s;

    Cmd_TokenizeString(text, true);

    // execute the command line
    if (!Cmd_Argc()) {
        return;        // no tokens
    }

    Com_DPrintf("stufftext: %s\n", text);

    s = Cmd_Argv(0);

    // handle private client commands
    if (!strcmp(s, "changing")) {
        CL_Changing_f();
        return;
    }
    if (!strcmp(s, "precache")) {
        CL_Precache_f();
        return;
    }

    // forbid nearly every command from demos
    if (cls.demo.playback) {
        if (strcmp(s, "play")) {
            return;
        }
    }

    // handle commands that are always allowed
    if (!strcmp(s, "reconnect")) {
        CL_Reconnect_f();
        return;
    }
    if (!strcmp(s, "cmd") && !cls.stufftextwhitelist) {
        CL_ForwardToServer_f();
        return;
    }

    if (cl_ignore_stufftext->integer >= 1 && !allow_stufftext(text)) {
        if (cl_ignore_stufftext->integer >= 2)
            Com_WPrintf("Ignored stufftext: %s\n", text);
        return;
    }

    // execute regular commands
    Cmd_ExecuteCommand(buf);
}

static void cl_gun_changed(cvar_t *self)
{
    CL_UpdateGunSetting();
}

static void info_hand_changed(cvar_t *self)
{
    CL_UpdateGunSetting();
}

static void cl_gibs_changed(cvar_t *self)
{
    CL_UpdateGibSetting();
}

static void cl_footsteps_changed(cvar_t *self)
{
    CL_UpdateFootstepsSetting();
}

static void cl_predict_changed(cvar_t *self)
{
    CL_UpdatePredictSetting();
}

#if USE_FPS
static void cl_updaterate_changed(cvar_t *self)
{
    CL_UpdateRateSetting();
}
#endif

static inline int fps_to_msec(int fps)
{
#if 0
    return (1000 + fps / 2) / fps;
#else
    return 1000 / fps;
#endif
}

static void warn_on_fps_rounding(cvar_t *cvar)
{
    static bool warned = false;
    int msec, real_maxfps;

    if (cvar->integer <= 0 || cl_warn_on_fps_rounding->integer <= 0)
        return;

    msec = fps_to_msec(cvar->integer);
    if (!msec)
        return;

    real_maxfps = 1000 / msec;
    if (cvar->integer == real_maxfps)
        return;

    Com_WPrintf("%s value `%d' is inexact, using `%d' instead.\n",
                cvar->name, cvar->integer, real_maxfps);
    if (!warned) {
        Com_Printf("(Set `%s' to `0' to disable this warning.)\n",
                   cl_warn_on_fps_rounding->name);
        warned = true;
    }
}

static void cl_sync_changed(cvar_t *self)
{
    CL_UpdateFrameTimes();
}

static void cl_maxfps_changed(cvar_t *self)
{
    CL_UpdateFrameTimes();
    warn_on_fps_rounding(self);
}

// allow downloads to be permanently disabled as a
// protection measure from malicious (or just stupid) servers
// that force downloads by stuffing commands
static void cl_allow_download_changed(cvar_t *self)
{
    if (self->integer == -1) {
        self->flags |= CVAR_ROM;
    }
}

// ugly hack for compatibility
static void cl_chat_sound_changed(cvar_t *self)
{
    if (!*self->string)
        self->integer = 0;
    else if (!Q_stricmp(self->string, "misc/talk.wav"))
        self->integer = 1;
    else if (!Q_stricmp(self->string, "misc/talk1.wav"))
        self->integer = 2;
    else if (!self->integer && !COM_IsUint(self->string))
        self->integer = 1;
}

#if USE_AQTION

static void cl_mk23_sound_changed(cvar_t *self)
{
    if (!Q_stricmp(self->string, "weapons/mk23fire0.wav"))
        self->integer = 0;
    else if (!Q_stricmp(self->string, "weapons/mk23fire1.wav"))
        self->integer = 1;
    else if (!Q_stricmp(self->string, "weapons/mk23fire2.wav"))
        self->integer = 2;
    else if (!self->integer && !COM_IsUint(self->string))
        self->integer = 0;
}

static void cl_mp5_sound_changed(cvar_t *self)
{
    if (!Q_stricmp(self->string, "weapons/mp5fire0.wav"))
        self->integer = 0;
    else if (!Q_stricmp(self->string, "weapons/mp5fire1.wav"))
        self->integer = 1;
    else if (!Q_stricmp(self->string, "weapons/mp5fire2.wav"))
        self->integer = 2;
    else if (!self->integer && !COM_IsUint(self->string))
        self->integer = 0;
}

static void cl_m4_sound_changed(cvar_t *self)
{
    if (!Q_stricmp(self->string, "weapons/m4a1fire0.wav"))
        self->integer = 0;
    else if (!Q_stricmp(self->string, "weapons/m4a1fire1.wav"))
        self->integer = 1;
    else if (!Q_stricmp(self->string, "weapons/m4a1fire2.wav"))
        self->integer = 2;
    else if (!self->integer && !COM_IsUint(self->string))
        self->integer = 0;
}

static void cl_m3_sound_changed(cvar_t *self)
{
    if (!Q_stricmp(self->string, "weapons/m3fire0.wav"))
        self->integer = 0;
    else if (!Q_stricmp(self->string, "weapons/m3fire1.wav"))
        self->integer = 1;
    else if (!Q_stricmp(self->string, "weapons/m3fire2.wav"))
        self->integer = 2;
    else if (!self->integer && !COM_IsUint(self->string))
        self->integer = 0;
}

static void cl_hc_sound_changed(cvar_t *self)
{
    if (!Q_stricmp(self->string, "weapons/hcfire0.wav"))
        self->integer = 0;
    else if (!Q_stricmp(self->string, "weapons/hcfire1.wav"))
        self->integer = 1;
    else if (!Q_stricmp(self->string, "weapons/hcfire2.wav"))
        self->integer = 2;
    else if (!self->integer && !COM_IsUint(self->string))
        self->integer = 0;
}

static void cl_ssg_sound_changed(cvar_t *self)
{
    if (!Q_stricmp(self->string, "weapons/ssgfire0.wav"))
        self->integer = 0;
    else if (!Q_stricmp(self->string, "weapons/ssgfire1.wav"))
        self->integer = 1;
    else if (!Q_stricmp(self->string, "weapons/ssgfire2.wav"))
        self->integer = 2;
    else if (!self->integer && !COM_IsUint(self->string))
        self->integer = 0;
}

#endif

void cl_timeout_changed(cvar_t *self)
{
    self->integer = 1000 * Cvar_ClampValue(self, 0, 24 * 24 * 60 * 60);
}

static const cmdreg_t c_client[] = {
    { "cmd", CL_ForwardToServer_f },
    { "pause", CL_Pause_f },
    { "pingservers", CL_PingServers_f },
    { "skins", CL_Skins_f },
    { "userinfo", CL_Userinfo_f },
    { "snd_restart", CL_RestartSound_f },
    { "play", CL_PlaySound_f, CL_PlaySound_c },
    { "disconnect", CL_Disconnect_f },
    { "connect", CL_Connect_f, CL_Connect_c },
    { "followip", CL_FollowIP_f },
    { "passive", CL_PassiveConnect_f },
    { "reconnect", CL_Reconnect_f },
    { "rcon", CL_Rcon_f, CL_Rcon_c },
    { "serverstatus", CL_ServerStatus_f, CL_ServerStatus_c },
    { "ignoretext", CL_IgnoreText_f },
    { "unignoretext", CL_UnIgnoreText_f },
    { "ignorenick", CL_IgnoreNick_f, CL_IgnoreNick_c },
    { "unignorenick", CL_UnIgnoreNick_f, CL_UnIgnoreNick_c },
    { "dumpclients", CL_DumpClients_f },
    { "dumpstatusbar", CL_DumpStatusbar_f },
    { "dumplayout", CL_DumpLayout_f },
    { "writeconfig", CL_WriteConfig_f, CL_WriteConfig_c },
    { "vid_restart", CL_RestartRefresh_f },
    { "r_reload", CL_ReloadRefresh_f },

    //
    // forward to server commands
    //
    // the only thing this does is allow command completion
    // to work -- all unknown commands are automatically
    // forwarded to the server
    { "say", NULL, CL_Say_c },
    { "say_team", NULL, CL_Say_c },

    { "wave" }, { "inven" }, { "kill" }, { "use" },
    { "drop" }, { "info" }, { "prog" },
    { "give" }, { "god" }, { "notarget" }, { "noclip" },
    { "invuse" }, { "invprev" }, { "invnext" }, { "invdrop" },
    { "weapnext" }, { "weapprev" },

    { NULL }
};

/*
=================
CL_InitLocal
=================
*/
static void CL_InitLocal(void)
{
    cvar_t *var;
    int i;

    cls.state = ca_disconnected;
    cls.connect_time -= CONNECT_INSTANT;

    CL_RegisterInput();
    CL_InitDemos();
    LOC_Init();
    CL_InitAscii();
    CL_InitEffects();
    CL_InitTEnts();
    CL_InitDownloads();
    CL_GTV_Init();

    List_Init(&cl_ignore_text);
    List_Init(&cl_ignore_nick);

    Cmd_Register(c_client);

    for (i = 0; i < MAX_LOCAL_SERVERS; i++) {
        var = Cvar_Get(va("adr%i", i), "", CVAR_ARCHIVE);
        var->generator = Com_Address_g;
    }

    //
    // register our variables
    //
    cl_gun = Cvar_Get("cl_gun", "1", 0);
    cl_gun->changed = cl_gun_changed;
    cl_gunalpha = Cvar_Get("cl_gunalpha", "1", 0);
    cl_gunfov = Cvar_Get("cl_gunfov", "90", 0);
    cl_gun_x = Cvar_Get("cl_gun_x", "0", 0);
    cl_gun_y = Cvar_Get("cl_gun_y", "0", 0);
    cl_gun_z = Cvar_Get("cl_gun_z", "0", 0);
    cl_footsteps = Cvar_Get("cl_footsteps", "1", 0);
    cl_footsteps->changed = cl_footsteps_changed;
    cl_noskins = Cvar_Get("cl_noskins", "0", 0);
    cl_noskins->changed = cl_noskins_changed;
    cl_predict = Cvar_Get("cl_predict", "1", 0);
    cl_predict->changed = cl_predict_changed;
	cl_predict_crouch = Cvar_Get("cl_predict_crouch", "1", 0);
    cl_kickangles = Cvar_Get("cl_kickangles", "1", CVAR_CHEAT);
    cl_warn_on_fps_rounding = Cvar_Get("cl_warn_on_fps_rounding", "1", 0);
    cl_maxfps = Cvar_Get("cl_maxfps", "60", 0);
    cl_maxfps->changed = cl_maxfps_changed;
    cl_async = Cvar_Get("cl_async", "1", 0);
    cl_async->changed = cl_sync_changed;
    r_maxfps = Cvar_Get("r_maxfps", "0", 0);
    r_maxfps->changed = cl_maxfps_changed;
    cl_autopause = Cvar_Get("cl_autopause", "1", 0);
    cl_rollhack = Cvar_Get("cl_rollhack", "1", 0);
    cl_noglow = Cvar_Get("cl_noglow", "0", 0);
    cl_nolerp = Cvar_Get("cl_nolerp", "0", 0);

    cl_new_movement_sounds = Cvar_Get("cl_new_movement_sounds", "0", 0);

#if USE_DISCORD && USE_CURL //rekkie -- discord -- s
    //rekkie -- external ip -- s
    cl_extern_ip = Cvar_Get("cl_extern_ip", "", CVAR_ROM);
    CL_GetExternalIP(); // Get external IP
    //rekkie -- external ip -- e
    
    cl_discord = Cvar_Get("cl_discord", "1", 0);
    discord.discord_found = true; // Defaults to true, until we find out otherwise.
    if (cl_discord->value)
    {
        cl_discord_id = Cvar_Get("cl_discord_id", "0", CVAR_ROM | CVAR_USERINFO);
        cl_discord_username = Cvar_Get("cl_discord_username", "", CVAR_ROM );
        cl_discord_discriminator = Cvar_Get("cl_discord_discriminator", "0", CVAR_ROM);
        cl_discord_avatar = Cvar_Get("cl_discord_avatar", "0", CVAR_ROM);
        cl_discord_accept_join_requests = Cvar_Get("cl_discord_accept_join_requests", "0", 0);
        CL_InitDiscord();
    }
#endif //rekkie -- discord -- e

    // hack for timedemo
    com_timedemo->changed = cl_sync_changed;

    CL_UpdateFrameTimes();
    warn_on_fps_rounding(cl_maxfps);
    warn_on_fps_rounding(r_maxfps);

#if USE_DEBUG
    cl_shownet = Cvar_Get("cl_shownet", "0", 0);
    cl_showmiss = Cvar_Get("cl_showmiss", "0", 0);
    cl_showclamp = Cvar_Get("showclamp", "0", 0);
#endif

    cl_timeout = Cvar_Get("cl_timeout", "120", 0);
    cl_timeout->changed = cl_timeout_changed;
    cl_timeout_changed(cl_timeout);

    rcon_address = Cvar_Get("rcon_address", "", CVAR_PRIVATE);
    rcon_address->generator = Com_Address_g;

    cl_thirdperson = Cvar_Get("cl_thirdperson", "0", CVAR_CHEAT);
    cl_thirdperson_angle = Cvar_Get("cl_thirdperson_angle", "0", 0);
    cl_thirdperson_range = Cvar_Get("cl_thirdperson_range", "60", 0);

    cl_disable_particles = Cvar_Get("cl_disable_particles", "0", 0);
    cl_disable_explosions = Cvar_Get("cl_disable_explosions", "0", 0);
    cl_gibs = Cvar_Get("cl_gibs", "1", 0);
    cl_gibs->changed = cl_gibs_changed;

#if USE_FPS
    cl_updaterate = Cvar_Get("cl_updaterate", "0", 0);
    cl_updaterate->changed = cl_updaterate_changed;
#endif

    cl_chat_notify = Cvar_Get("cl_chat_notify", "1", 0);
    cl_chat_sound = Cvar_Get("cl_chat_sound", "1", 0);
    cl_chat_sound->changed = cl_chat_sound_changed;
    cl_chat_sound_changed(cl_chat_sound);
    cl_chat_filter = Cvar_Get("cl_chat_filter", "0", 0);

    cl_disconnectcmd = Cvar_Get("cl_disconnectcmd", "", 0);
    cl_changemapcmd = Cvar_Get("cl_changemapcmd", "", 0);
    cl_beginmapcmd = Cvar_Get("cl_beginmapcmd", "", 0);

    cl_ignore_stufftext = Cvar_Get("cl_ignore_stufftext", "0", 0);

    cl_protocol = Cvar_Get("cl_protocol", "0", 0);

    gender_auto = Cvar_Get("gender_auto", "1", CVAR_ARCHIVE);

    cl_vwep = Cvar_Get("cl_vwep", "1", CVAR_ARCHIVE);
    cl_vwep->changed = cl_vwep_changed;

    allow_download->changed = cl_allow_download_changed;
    cl_allow_download_changed(allow_download);

    //
    // userinfo
    //
    info_password = Cvar_Get("password", "", CVAR_USERINFO);
    info_spectator = Cvar_Get("spectator", "0", CVAR_USERINFO);
    info_name = Cvar_Get("name", "unnamed", CVAR_USERINFO | CVAR_ARCHIVE);
    info_skin = Cvar_Get("skin", "male/grunt", CVAR_USERINFO | CVAR_ARCHIVE);
    info_rate = Cvar_Get("rate", "5000", CVAR_USERINFO | CVAR_ARCHIVE);
    info_msg = Cvar_Get("msg", "1", CVAR_USERINFO | CVAR_ARCHIVE);
    info_hand = Cvar_Get("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);
    info_hand->changed = info_hand_changed;
    info_fov = Cvar_Get("fov", "90", CVAR_USERINFO | CVAR_ARCHIVE);
    info_gender = Cvar_Get("gender", "male", CVAR_USERINFO | CVAR_ARCHIVE);
    info_gender->modified = false; // clear this so we know when user sets it manually
    info_uf = Cvar_Get("uf", "", CVAR_USERINFO);
    #if USE_CLIENT
        info_steamid = Cvar_Get("steamid", "", CVAR_USERINFO);
        info_steamcloudappenabled = Cvar_Get("steamcloudappenabled", "", CVAR_USERINFO);
        info_steamclouduserenabled = Cvar_Get("steamclouduserenabled", "", CVAR_USERINFO);
    #endif
    info_version = Cvar_Get("version", "", CVAR_USERINFO);

    #if USE_AQTION
        cl_mk23_sound = Cvar_Get("cl_mk23_sound", "0", 0);
        cl_mk23_sound->changed = cl_mk23_sound_changed;
        cl_mk23_sound_changed(cl_mk23_sound);

        cl_mp5_sound = Cvar_Get("cl_mp5_sound", "0", 0);
        cl_mp5_sound->changed = cl_mp5_sound_changed;
        cl_mp5_sound_changed(cl_mp5_sound);

        cl_m4_sound = Cvar_Get("cl_m4_sound", "0", 0);
        cl_m4_sound->changed = cl_m4_sound_changed;
        cl_m4_sound_changed(cl_m4_sound);

        cl_m3_sound = Cvar_Get("cl_m3_sound", "0", 0);
        cl_m3_sound->changed = cl_m3_sound_changed;
        cl_m3_sound_changed(cl_m3_sound);

        cl_hc_sound = Cvar_Get("cl_hc_sound", "0", 0);
        cl_hc_sound->changed = cl_hc_sound_changed;
        cl_hc_sound_changed(cl_hc_sound);

        cl_ssg_sound = Cvar_Get("cl_ssg_sound", "0", 0);
        cl_ssg_sound->changed = cl_ssg_sound_changed;
        cl_ssg_sound_changed(cl_ssg_sound);

    #endif


    //
    // macros
    //
    Cmd_AddMacro("cl_mapname", CL_Mapname_m);
    Cmd_AddMacro("cl_server", CL_Server_m);
    Cmd_AddMacro("cl_timer", CL_Timer_m);
    Cmd_AddMacro("cl_demopos", CL_DemoPos_m);
    Cmd_AddMacro("cl_ups", CL_Ups_m);
    Cmd_AddMacro("cl_fps", CL_Fps_m);
    Cmd_AddMacro("r_fps", R_Fps_m);
    Cmd_AddMacro("cl_mps", CL_Mps_m);   // moves per second
    Cmd_AddMacro("cl_pps", CL_Pps_m);   // packets per second
    Cmd_AddMacro("cl_ping", CL_Ping_m);
    Cmd_AddMacro("cl_lag", CL_Lag_m);
    Cmd_AddMacro("cl_health", CL_Health_m);
    Cmd_AddMacro("cl_ammo", CL_Ammo_m);
    Cmd_AddMacro("cl_armor", CL_Armor_m);
    Cmd_AddMacro("cl_weaponmodel", CL_WeaponModel_m);
}

/*
==================
CL_CheatsOK
==================
*/
bool CL_CheatsOK(void)
{
    // can cheat when disconnected or playing a demo
    if (cls.state < ca_connected || cls.demo.playback)
        return true;

    // can't cheat on remote servers
    if (!sv_running->integer)
        return false;

    // developer option
    if (Cvar_VariableInteger("cheats"))
        return true;

    // single player can cheat
    if (cls.state > ca_connected && cl.maxclients == 1)
        return true;

    // can cheat when playing MVD
    if (MVD_GetDemoStatus(NULL, NULL, NULL))
        return true;

    return false;
}

//============================================================================

/*
==================
CL_Activate
==================
*/
void CL_Activate(active_t active)
{
    if (cls.active != active) {
        Com_DDPrintf("%s: %u\n", __func__, active);
        cls.active = active;
        cls.disable_screen = 0;
        Key_ClearStates();
        IN_Activate();
        S_Activate();
        CL_UpdateFrameTimes();
    }
}

static void CL_SetClientTime(void)
{
    int prevtime;

    if (com_timedemo->integer) {
        cl.time = cl.servertime;
        cl.lerpfrac = 1.0f;
#if USE_FPS
        cl.keytime = cl.keyservertime;
        cl.keylerpfrac = 1.0f;
#endif
        return;
    }

    prevtime = cl.servertime - CL_FRAMETIME;
    if (cl.time > cl.servertime) {
        SHOWCLAMP(1, "high clamp %i\n", cl.time - cl.servertime);
        cl.time = cl.servertime;
        cl.lerpfrac = 1.0f;
    } else if (cl.time < prevtime) {
        SHOWCLAMP(1, "low clamp %i\n", prevtime - cl.time);
        cl.time = prevtime;
        cl.lerpfrac = 0;
    } else {
        cl.lerpfrac = (cl.time - prevtime) * CL_1_FRAMETIME;
    }

    SHOWCLAMP(2, "time %d %d, lerpfrac %.3f\n",
              cl.time, cl.servertime, cl.lerpfrac);

#if USE_FPS
    prevtime = cl.keyservertime - BASE_FRAMETIME;
    if (cl.keytime > cl.keyservertime) {
        SHOWCLAMP(1, "high keyclamp %i\n", cl.keytime - cl.keyservertime);
        cl.keytime = cl.keyservertime;
        cl.keylerpfrac = 1.0f;
    } else if (cl.keytime < prevtime) {
        SHOWCLAMP(1, "low keyclamp %i\n", prevtime - cl.keytime);
        cl.keytime = prevtime;
        cl.keylerpfrac = 0;
    } else {
        cl.keylerpfrac = (cl.keytime - prevtime) * BASE_1_FRAMETIME;
    }

    SHOWCLAMP(2, "keytime %d %d keylerpfrac %.3f\n",
              cl.keytime, cl.keyservertime, cl.keylerpfrac);
#endif
}

static void CL_MeasureStats(void)
{
    int i;

    if (com_localTime - cls.measure.time < 1000) {
        return;
    }

    // measure average ping
    if (cls.netchan.protocol) {
        int ack = cls.netchan.incoming_acknowledged;
        int ping = 0;
        int j, k = 0;

        i = ack - 16 + 1;
        if (i < cl.initialSeq) {
            i = cl.initialSeq;
        }
        for (j = i; j <= ack; j++) {
            client_history_t *h = &cl.history[j & CMD_MASK];
            if (h->rcvd > h->sent) {
                ping += h->rcvd - h->sent;
                k++;
            }
        }

        cls.measure.ping = k ? ping / k : 0;
    }

    // measure main/refresh frame counts
    for (i = 0; i < 4; i++) {
        cls.measure.fps[i] = cls.measure.frames[i];
        cls.measure.frames[i] = 0;
    }

    cls.measure.time = com_localTime;
}

#if USE_AUTOREPLY
static void CL_CheckForReply(void)
{
    if (!cl.reply_delta) {
        return;
    }

    if (cls.realtime - cl.reply_time < cl.reply_delta) {
        return;
    }

    CL_ClientCommand(va("say \"%s\"", com_version->string));

    cl.reply_delta = 0;
}
#endif

static void CL_CheckTimeout(void)
{
    if (!cls.netchan.protocol) {
        return;
    }
    if (NET_IsLocalAddress(&cls.netchan.remote_address)) {
        return;
    }

#if USE_ICMP
    if (cls.errorReceived && com_localTime - cls.netchan.last_received > 5000) {
        Com_Error(ERR_DISCONNECT, "Server connection was reset.");
    }
#endif

    if (cl_timeout->integer && com_localTime - cls.netchan.last_received > cl_timeout->integer) {
        // timeoutcount saves debugger
        if (++cl.timeoutcount > 5) {
            Com_Error(ERR_DISCONNECT, "Server connection timed out.");
        }
    } else {
        cl.timeoutcount = 0;
    }
}

/*
=================
CL_CheckForPause

=================
*/
void CL_CheckForPause(void)
{
    if (cls.state != ca_active) {
        // only pause when active
        Cvar_Set("cl_paused", "0");
        Cvar_Set("sv_paused", "0");
        return;
    }

    if (cls.key_dest & (KEY_CONSOLE | KEY_MENU)) {
        // only pause in single player
        if (cl_paused->integer == 0 && cl_autopause->integer) {
            Cvar_Set("cl_paused", "1");
        }
    } else if (cl_paused->integer == 1) {
        // only resume after automatic pause
        Cvar_Set("cl_paused", "0");
    }

    // hack for demo playback pause/unpause
    if (cls.demo.playback) {
        // don't pause when running timedemo!
        if (cl_paused->integer && !com_timedemo->integer) {
            if (!sv_paused->integer) {
                Cvar_Set("sv_paused", "1");
                IN_Activate();
            }
        } else {
            if (sv_paused->integer) {
                Cvar_Set("sv_paused", "0");
                IN_Activate();
            }
        }
    }
}

typedef enum {
    SYNC_TIMEDEMO,
    SYNC_MAXFPS,
    SYNC_SLEEP_10,
    SYNC_SLEEP_60,
    ASYNC_FULL
} sync_mode_t;

#if USE_DEBUG
static const char *const sync_names[] = {
    "SYNC_TIMEDEMO",
    "SYNC_MAXFPS",
    "SYNC_SLEEP_10",
    "SYNC_SLEEP_60",
    "ASYNC_FULL"
};
#endif

static int ref_msec, phys_msec, main_msec;
static int ref_extra, phys_extra, main_extra;
static sync_mode_t sync_mode;

#define MIN_PHYS_HZ 10
#define MAX_PHYS_HZ 250
#define MIN_REF_HZ MIN_PHYS_HZ
#define MAX_REF_HZ 1000

static int fps_to_clamped_msec(cvar_t *cvar, int min, int max)
{
    if (cvar->integer == 0)
        return fps_to_msec(max);
    else
        return fps_to_msec(Cvar_ClampInteger(cvar, min, max));
}

/*
==================
CL_UpdateFrameTimes

Called whenever async/fps cvars change, but not every frame
==================
*/
void CL_UpdateFrameTimes(void)
{
    if (!cls.state) {
        return; // not yet fully initialized
    }

    phys_msec = ref_msec = main_msec = 0;
    ref_extra = phys_extra = main_extra = 0;

    if (com_timedemo->integer) {
        // timedemo just runs at full speed
        sync_mode = SYNC_TIMEDEMO;
    } else if (cls.active == ACT_MINIMIZED) {
        // run at 10 fps if minimized
        main_msec = fps_to_msec(10);
        sync_mode = SYNC_SLEEP_10;
    } else if (cls.active == ACT_RESTORED || cls.state != ca_active) {
        // run at 60 fps if not active
        main_msec = fps_to_msec(60);
        sync_mode = SYNC_SLEEP_60;
    } else if (cl_async->integer > 0) {
        // run physics and refresh separately
        phys_msec = fps_to_clamped_msec(cl_maxfps, MIN_PHYS_HZ, MAX_PHYS_HZ);
        ref_msec = fps_to_clamped_msec(r_maxfps, MIN_REF_HZ, MAX_REF_HZ);
        sync_mode = ASYNC_FULL;
    } else {
        // everything ticks in sync with refresh
        main_msec = fps_to_clamped_msec(cl_maxfps, MIN_PHYS_HZ, MAX_PHYS_HZ);
        sync_mode = SYNC_MAXFPS;
    }

    Com_DDPrintf("%s: mode=%s main_msec=%d ref_msec=%d, phys_msec=%d\n",
                 __func__, sync_names[sync_mode], main_msec, ref_msec, phys_msec);
}

/*
==================
CL_Frame

==================
*/
unsigned CL_Frame(unsigned msec)
{
    bool phys_frame = true, ref_frame = true;

    time_after_ref = time_before_ref = 0;

    if (!cl_running->integer) {
        return UINT_MAX;
    }

    main_extra += msec;
    cls.realtime += msec;

    CL_ProcessEvents();

    switch (sync_mode) {
    case SYNC_TIMEDEMO:
        // timedemo just runs at full speed
        break;
    case SYNC_SLEEP_10:
        // don't run refresh at all
        ref_frame = false;
        // fall through
    case SYNC_SLEEP_60:
        // run at limited fps if not active
        if (main_extra < main_msec) {
            return main_msec - main_extra;
        }
        break;
    case ASYNC_FULL:
        // run physics and refresh separately
        phys_extra += main_extra;
        ref_extra += main_extra;

        if (phys_extra < phys_msec) {
            phys_frame = false;
        } else if (phys_extra > phys_msec * 4) {
            phys_extra = phys_msec;
        }

        if (ref_extra < ref_msec) {
            ref_frame = false;
        } else if (ref_extra > ref_msec * 4) {
            ref_extra = ref_msec;
        }
        break;
    case SYNC_MAXFPS:
        // everything ticks in sync with refresh
        if (main_extra < main_msec) {
            if (!cl.sendPacketNow) {
                return 0;
            }
            ref_frame = false;
        }
        break;
    }

    Com_DDDDPrintf("main_extra=%d ref_frame=%d ref_extra=%d "
                   "phys_frame=%d phys_extra=%d\n",
                   main_extra, ref_frame, ref_extra,
                   phys_frame, phys_extra);

    // decide the simulation time
    cls.frametime = main_extra * 0.001f;

    if (cls.frametime > 1.0f / 5)
        cls.frametime = 1.0f / 5;

    if (!sv_paused->integer) {
        cl.time += main_extra;
#if USE_FPS
        cl.keytime += main_extra;
#endif
    }

    // read next demo frame
    if (cls.demo.playback)
        CL_DemoFrame(main_extra);

    // calculate local time
    if (cls.state == ca_active && !sv_paused->integer)
        CL_SetClientTime();

#if USE_AUTOREPLY
    // check for version reply
    CL_CheckForReply();
#endif

    // resend a connection request if necessary
    CL_CheckForResend();

    // read user intentions
    CL_UpdateCmd(main_extra);

    // finalize pending cmd
    phys_frame |= cl.sendPacketNow;
    if (phys_frame) {
        CL_FinalizeCmd();
        phys_extra -= phys_msec;
        M_FRAMES++;

        // don't let the time go too far off
        // this can happen due to cl.sendPacketNow
        if (phys_extra < -phys_msec * 4) {
            phys_extra = 0;
        }
    }

    // send pending cmds
    CL_SendCmd();

    // predict all unacknowledged movements
    CL_PredictMovement();

    Con_RunConsole();

    SCR_RunCinematic();

    UI_Frame(main_extra);

    if (ref_frame) {
        // update the screen
        if (host_speeds->integer)
            time_before_ref = Sys_Milliseconds();

        SCR_UpdateScreen();

        if (host_speeds->integer)
            time_after_ref = Sys_Milliseconds();

        ref_extra -= ref_msec;
        R_FRAMES++;

        // update audio after the 3D view was drawn
        S_Update();
    } else if (sync_mode == SYNC_SLEEP_10) {
        // force audio and effects update if not rendering
        CL_CalcViewValues();
        S_Update();
    }

    // check connection timeout
    CL_CheckTimeout();

    C_FRAMES++;

    CL_MeasureStats();

    cls.framecount++;

    main_extra = 0;
    return 0;
}

/*
============
CL_ProcessEvents
============
*/
bool CL_ProcessEvents(void)
{
    if (!cl_running->integer) {
        return false;
    }

    CL_RunRefresh();

    IN_Frame();

    NET_GetPackets(NS_CLIENT, CL_PacketEvent);

    // process console and stuffed commands
    Cbuf_Execute(&cmd_buffer);
    Cbuf_Execute(&cl_cmdbuf);

    HTTP_RunDownloads();

    CL_GTV_Run();

#if USE_DISCORD && USE_CURL //rekkie -- discord -- s
    CL_RunDiscord();
#endif //rekkie -- discord -- e

    return cl.sendPacketNow;
}

//============================================================================

/*
====================
CL_Init
====================
*/
void CL_Init(void)
{
    if (dedicated->integer) {
        return; // nothing running on the client
    }

    if (cl_running->integer) {
        return;
    }

    // all archived variables will now be loaded

    // start with full screen console
    cls.key_dest = KEY_CONSOLE;

    CL_InitRefresh();
    S_Init();   // sound must be initialized after window is created

    CL_InitLocal();
    IN_Init();

#if USE_ZLIB
    Q_assert(inflateInit2(&cls.z, -MAX_WBITS) == Z_OK);
#endif

    OGG_Init();
    CL_LoadDownloadIgnores();
    CL_LoadStuffTextWhiteList();

    HTTP_Init();

    UI_OpenMenu(UIMENU_DEFAULT);

    Con_PostInit();
    Con_RunConsole();

    cl_cmdbuf.from = FROM_STUFFTEXT;
    cl_cmdbuf.text = cl_cmdbuf_text;
    cl_cmdbuf.maxsize = sizeof(cl_cmdbuf_text);
    cl_cmdbuf.exec = exec_server_string;

    Cvar_Set("cl_running", "1");
}

/*
===============
CL_Shutdown

FIXME: this is a callback from Com_Quit and Com_Error.  It would be better
to run quit through here before the final handoff to the sys code.
===============
*/
void CL_Shutdown(void)
{
    static bool isdown = false;

    if (isdown) {
        Com_Printf("CL_Shutdown: recursive shutdown\n");
        return;
    }
    isdown = true;

    if (!cl_running || !cl_running->integer) {
        return;
    }

    CL_GTV_Shutdown();

#if USE_DISCORD && USE_CURL //rekkie -- discord -- s
    if (discord.init)
        CL_ShutdownDiscord();
#endif //rekkie -- discord -- e

    CL_Disconnect(ERR_FATAL);

#if USE_ZLIB
    inflateEnd(&cls.z);
#endif

    HTTP_Shutdown();
    OGG_Shutdown();
    S_Shutdown();
    IN_Shutdown();
    Con_Shutdown();
    CL_ShutdownRefresh();
    CL_WriteConfig();

    memset(&cls, 0, sizeof(cls));

    Cvar_Set("cl_running", "0");

    isdown = false;
}
