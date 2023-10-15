/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2003-2006 Andrey Nazarov

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

//
// images.h -- common image manager
//

#include "shared/list.h"
#include "common/files.h"
#include "common/zone.h"
#include "common/error.h"
#include "refresh/refresh.h"

#define IMG_AllocPixels(x)  FS_AllocTempMem(x)
#define IMG_FreePixels(x)   FS_FreeTempMem(x)

#define LUMINANCE(r, g, b) ((r) * 0.2126f + (g) * 0.7152f + (b) * 0.0722f)

#define U32_ALPHA   MakeColor(  0,   0,   0, 255)
#define U32_RGB     MakeColor(255, 255, 255,   0)

// absolute limit for OpenGL renderer
#define MAX_TEXTURE_SIZE    4096

typedef enum {
    IM_PCX,
    IM_WAL,
#if USE_TGA
    IM_TGA,
#endif
#if USE_JPG
    IM_JPG,
#endif
#if USE_PNG
    IM_PNG,
#endif
    IM_MAX
} imageformat_t;

typedef struct image_s {
    list_t          entry;
    char            name[MAX_QPATH]; // game path
    int             baselen; // without extension
    imagetype_t     type;
    imageflags_t    flags;
    int16_t         width, height; // source image
    int16_t         upload_width, upload_height; // after power of two and picmip
    int             registration_sequence; // 0 = free
    unsigned        texnum, glow_texnum; // gl texture binding
    float           sl, sh, tl, th;
    float           aspect;
} image_t;

#define MAX_RIMAGES     8192

extern image_t  r_images[MAX_RIMAGES];
extern int      r_numImages;

extern int registration_sequence;

#define R_NOTEXTURE    &r_images[0]
#define R_BLACKTEXTURE &r_images[5]

extern uint32_t d_8to24table[256];

image_t *IMG_Find(const char *name, imagetype_t type, imageflags_t flags);
void IMG_FreeUnused(void);
void IMG_FreeAll(void);
void IMG_Init(void);
void IMG_Shutdown(void);
void IMG_GetPalette(void);

image_t *IMG_ForHandle(qhandle_t h);

void IMG_Unload(image_t *image);
void IMG_LoadRaw(image_t *image, byte *pic, int width, int height);
void IMG_Load(image_t *image, byte *pic);

struct screenshot_s;

typedef int (*save_cb_t)(struct screenshot_s *restrict);

typedef struct screenshot_s {
    save_cb_t save_cb;
    byte *pixels;
    FILE *fp;
    char *filename;
    int width, height, rowbytes, bpp, status, param;
    bool async;
} screenshot_t;

void IMG_ReadPixels(screenshot_t *s);
