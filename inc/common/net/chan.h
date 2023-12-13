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

#include "common/msg.h"
#include "common/net/net.h"
#include "common/sizebuf.h"

typedef enum {
    NETCHAN_OLD,
    NETCHAN_NEW
} netchan_type_t;

typedef struct {
    netchan_type_t  type;
    int         protocol;
    unsigned    maxpacketlen;

    netsrc_t    sock;

    int         dropped;            // between last packet and previous
    unsigned    total_dropped;      // for statistics
    unsigned    total_received;

    unsigned    last_received;      // for timeouts
    unsigned    last_sent;          // for retransmits

    int         qport;              // qport value to write when transmitting
    netadr_t    remote_address;

    sizebuf_t   message;            // writing buffer for reliable data

    unsigned    reliable_length;

    bool        reliable_ack_pending;   // set to true each time reliable is received
    bool        fragment_pending;

    // sequencing variables
    int         incoming_sequence;
    int         incoming_acknowledged;
    int         outgoing_sequence;

    bool        incoming_reliable_acknowledged; // single bit
    bool        incoming_reliable_sequence;     // single bit, maintained local
    bool        reliable_sequence;          // single bit
    int         last_reliable_sequence;     // sequence number of last send
    int         fragment_sequence;

    // message is copied to this buffer when it is first transfered
    byte        *reliable_buf;      // unacked reliable message

    sizebuf_t   fragment_in;
    sizebuf_t   fragment_out;
} netchan_t;

extern cvar_t       *net_qport;
extern cvar_t       *net_maxmsglen;
extern cvar_t       *net_chantype;

void Netchan_Init(void);
void Netchan_OutOfBand(netsrc_t sock, const netadr_t *adr,
                       const char *format, ...) q_printf(3, 4);
void Netchan_Setup(netchan_t *chan, netsrc_t sock, netchan_type_t type,
                   const netadr_t *adr, int qport, size_t maxpacketlen, int protocol);
int Netchan_Transmit(netchan_t *chan, size_t length, const void *data, int numpackets);
int Netchan_TransmitNextFragment(netchan_t *chan);
bool Netchan_Process(netchan_t *chan);
bool Netchan_ShouldUpdate(netchan_t *chan);
void Netchan_Close(netchan_t *chan);

#define OOB_PRINT(sock, addr, data) \
    NET_SendPacket(sock, CONST_STR_LEN("\xff\xff\xff\xff" data), addr)
