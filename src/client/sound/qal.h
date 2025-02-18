/*
Copyright (C) 2022 Andrey Nazarov

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

#ifndef QAL_HARD_LINKED
#define AL_NO_PROTOTYPES
#endif

#include <AL/al.h>
#include <AL/alext.h>
#include <AL/efx.h>

#ifndef QALAPI
#define QALAPI extern
#endif

#ifndef AL_SOFT_direct_channels_remix
#define AL_REMIX_UNMATCHED_SOFT     0x0002
#endif

#ifdef QAL_HARD_LINKED
#define qalBufferData alBufferData
#define qalBufferiv alBufferiv
#define qalDeleteBuffers alDeleteBuffers
#define qalDeleteSources alDeleteSources
#define qalDisable alDisable
#define qalDistanceModel alDistanceModel
#define qalEnable alEnable
#define qalGenBuffers alGenBuffers
#define qalGenSources alGenSources
#define qalGetEnumValue alGetEnumValue
#define qalGetError alGetError
#define qalGetProcAddress alGetProcAddress
#define qalGetSourcef alGetSourcef
#define qalGetSourcei alGetSourcei
#define qalGetString alGetString
#define qalIsExtensionPresent alIsExtensionPresent
#define qalListener3f alListener3f
#define qalListenerf alListenerf
#define qalListenerfv alListenerfv
#define qalSource3f alSource3f
#define qalSourcePlay alSourcePlay
#define qalSourceQueueBuffers alSourceQueueBuffers
#define qalSourceStop alSourceStop
#define qalSourceUnqueueBuffers alSourceUnqueueBuffers
#define qalSourcef alSourcef
#define qalSourcei alSourcei
#else
// AL 1.1
QALAPI LPALBUFFERDATA qalBufferData;
QALAPI LPALBUFFERIV qalBufferiv;
QALAPI LPALDELETEBUFFERS qalDeleteBuffers;
QALAPI LPALDELETESOURCES qalDeleteSources;
QALAPI LPALDISABLE qalDisable;
QALAPI LPALDISTANCEMODEL qalDistanceModel;
QALAPI LPALENABLE qalEnable;
QALAPI LPALGENBUFFERS qalGenBuffers;
QALAPI LPALGENSOURCES qalGenSources;
QALAPI LPALGETENUMVALUE qalGetEnumValue;
QALAPI LPALGETERROR qalGetError;
QALAPI LPALGETPROCADDRESS qalGetProcAddress;
QALAPI LPALGETSOURCEF qalGetSourcef;
QALAPI LPALGETSOURCEI qalGetSourcei;
QALAPI LPALGETSTRING qalGetString;
QALAPI LPALISEXTENSIONPRESENT qalIsExtensionPresent;
QALAPI LPALLISTENER3F qalListener3f;
QALAPI LPALLISTENERF qalListenerf;
QALAPI LPALLISTENERFV qalListenerfv;
QALAPI LPALSOURCE3F qalSource3f;
QALAPI LPALSOURCEPLAY qalSourcePlay;
QALAPI LPALSOURCEQUEUEBUFFERS qalSourceQueueBuffers;
QALAPI LPALSOURCESTOP qalSourceStop;
QALAPI LPALSOURCEUNQUEUEBUFFERS qalSourceUnqueueBuffers;
QALAPI LPALSOURCEF qalSourcef;
QALAPI LPALSOURCEI qalSourcei;
#endif

// ALC_EXT_EFX
QALAPI LPALDELETEFILTERS qalDeleteFilters;
QALAPI LPALFILTERF qalFilterf;
QALAPI LPALFILTERI qalFilteri;
QALAPI LPALGENFILTERS qalGenFilters;

bool QAL_Init(void);
void QAL_Shutdown(void);
