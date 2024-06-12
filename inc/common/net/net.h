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

#pragma once

#include "common/fifo.h"

// net.h -- quake's interface to the networking layer

#define PORT_ANY            -1
#define PORT_MASTER         27900
#define PORT_SERVER         27910

#define MIN_PACKETLEN                   512     // don't allow smaller packets
#define MAX_PACKETLEN                   4096    // max length of a single packet
#define PACKET_HEADER                   10      // two ints and a short (worst case)
#define MAX_PACKETLEN_DEFAULT           1400    // default quake2 limit
#define MAX_PACKETLEN_WRITABLE          (MAX_PACKETLEN - PACKET_HEADER)
#define MAX_PACKETLEN_WRITABLE_DEFAULT  (MAX_PACKETLEN_DEFAULT - PACKET_HEADER)

#ifdef _WIN32
typedef intptr_t qsocket_t;
#else
typedef int qsocket_t;
#endif

struct pollfd;

// portable network error codes
typedef enum {
    NET_OK      =  0,   // success
    NET_ERROR   = -1,   // failure (NET_ErrorString returns error message)
    NET_AGAIN   = -2,   // operation would block, try again
    NET_CLOSED  = -3,   // peer has closed connection
} neterr_t;

typedef enum {
    NA_UNSPECIFIED,
#if USE_CLIENT
    NA_LOOPBACK,
    NA_BROADCAST,
#endif
    NA_IP,
    NA_IP6
} netadrtype_t;

typedef enum {
    NS_CLIENT,
    NS_SERVER,
    NS_COUNT
} netsrc_t;

typedef enum {
    NET_NONE    = 0,
    NET_CLIENT  = BIT(0),
    NET_SERVER  = BIT(1)
} netflag_t;

typedef union {
    uint8_t u8[16];
    uint16_t u16[8];
    uint32_t u32[4];
    uint64_t u64[2];
} netadrip_t;

typedef struct {
    netadrtype_t type;
    netadrip_t ip;
    uint16_t port;
    uint32_t scope_id;  // IPv6 crap
} netadr_t;

typedef enum {
    NS_DISCONNECTED,// no socket opened
    NS_CONNECTING,  // connect() not yet completed
    NS_CONNECTED,   // may transmit data
    NS_CLOSED,      // peer has preformed orderly shutdown
    NS_BROKEN       // fatal error has been signaled
} netstate_t;

typedef struct {
    struct pollfd *socket;
    netadr_t address;
    netstate_t state;
    fifo_t recv;
    fifo_t send;
} netstream_t;

#if USE_CLIENT
#define     NET_IsLocalAddress(adr)     ((adr)->type == NA_LOOPBACK)
#else
#define     NET_IsLocalAddress(adr)     false
#endif

static inline bool NET_IsEqualAdr(const netadr_t *a, const netadr_t *b)
{
    if (a->type != b->type) {
        return false;
    }

    if (NET_IsLocalAddress(a)) {
        return true;
    }

    switch (a->type) {
    case NA_IP:
        return a->ip.u32[0] == b->ip.u32[0] && a->port == b->port;
    case NA_IP6:
        return !memcmp(a->ip.u8, b->ip.u8, 16) && a->port == b->port;
    default:
        return false;
    }
}

static inline bool NET_IsEqualBaseAdr(const netadr_t *a, const netadr_t *b)
{
    if (a->type != b->type) {
        return false;
    }

    if (NET_IsLocalAddress(a)) {
        return true;
    }

    switch (a->type) {
    case NA_IP:
        return a->ip.u32[0] == b->ip.u32[0];
    case NA_IP6:
        return !memcmp(a->ip.u8, b->ip.u8, 16);
    default:
        return false;
    }
}

static inline bool NET_IsEqualBaseAdrMask(const netadr_t *a,
                                          const netadr_t *b,
                                          const netadr_t *m)
{
    if (a->type != b->type) {
        return false;
    }

    switch (a->type) {
    case NA_IP:
        return !((a->ip.u32[0] ^ b->ip.u32[0]) & m->ip.u32[0]);
    case NA_IP6:
        return !(((a->ip.u64[0] ^ b->ip.u64[0]) & m->ip.u64[0]) |
                 ((a->ip.u64[1] ^ b->ip.u64[1]) & m->ip.u64[1]));
    default:
        return false;
    }
}

static inline bool NET_IsLanAddress(const netadr_t *adr)
{
    if (NET_IsLocalAddress(adr)) {
        return true;
    }

    switch (adr->type) {
    case NA_IP:
        return adr->ip.u8[0] == 127 || adr->ip.u8[0] == 10 ||
            adr->ip.u16[0] == MakeRawShort(192, 168) ||
            adr->ip.u16[0] == MakeRawShort(172,  16);
    case NA_IP6:
        return adr->ip.u8[0] == 0xfe && (adr->ip.u8[1] & 0xc0) == 0x80;
    default:
        return false;
    }
}

void        NET_Init(void);
void        NET_Shutdown(void);
void        NET_Config(netflag_t flag);
void        NET_UpdateStats(void);

bool        NET_GetAddress(netsrc_t sock, netadr_t *adr);
void        NET_GetPackets(netsrc_t sock, void (*packet_cb)(void));
bool        NET_SendPacket(netsrc_t sock, const void *data,
                           size_t len, const netadr_t *to);

const char  *NET_AdrToString(const netadr_t *a);
bool        NET_StringToAdr(const char *s, netadr_t *a, int default_port);
bool        NET_StringPairToAdr(const char *host, const char *port, netadr_t *a);

const char  *NET_BaseAdrToString(const netadr_t *a);
#define     NET_StringToBaseAdr(s, a)   NET_StringPairToAdr(s, NULL, a)

const char  *NET_ErrorString(void);

void        NET_CloseStream(netstream_t *s);
neterr_t    NET_Listen(bool listen);
neterr_t    NET_Accept(netstream_t *s);
neterr_t    NET_Connect(const netadr_t *peer, netstream_t *s);
neterr_t    NET_RunConnect(netstream_t *s);
neterr_t    NET_RunStream(netstream_t *s);
void        NET_UpdateStream(netstream_t *s);

struct pollfd   *NET_AllocPollFd(void);
void            NET_FreePollFd(struct pollfd *e);

int         NET_Sleep(int msec);
#if USE_AC_SERVER
int         NET_Sleep1(int msec, struct pollfd *e);
#endif

extern cvar_t       *net_ip;
extern cvar_t       *net_port;

extern netadr_t     net_from;
