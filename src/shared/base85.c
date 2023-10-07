/*
The MIT License (MIT)

Copyright (c) 2015 Judson Weissert

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "shared/base85.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define dimof(x) (sizeof(x) / sizeof(*x))

/// Base85 decode array. Zero indicates an invalid entry.
/// @see base85_decode_init()
#if defined (B85_ZEROMQ)
#define B85_G_DECODE g_z85_decode
#else
#define B85_G_DECODE g_ascii85_decode
#endif

static uint8_t B85_G_DECODE[256];

static const uint8_t B85_HEADER0 = '<';
static const uint8_t B85_HEADER1 = '~';
static const uint8_t B85_FOOTER0 = '~';
static const uint8_t B85_FOOTER1 = '>';
static const uint8_t B85_ZERO_CHAR = 'z';

typedef enum
{
  B85_S_START = 0,
  B85_S_NO_HEADER,
  B85_S_HEADER0,
  B85_S_HEADER,
  B85_S_FOOTER0,
  B85_S_FOOTER,
  B85_S_INVALID,
  B85_S_END
} b85_state_t;

/// State transitions for handling ascii85 header/footer.
static bool
base85_handle_state (uint8_t c, struct base85_context_t *ctx)
{
  uint8_t *state = &ctx->state;
  switch (*state)
  {
  case B85_S_START:
    if (B85_HEADER0 == c)
    {
      *state = B85_S_HEADER0;
      return true;
    }

    *state = B85_S_NO_HEADER;
    return false;

  case B85_S_NO_HEADER:
    return false;

  case B85_S_HEADER0:
    if (B85_HEADER1 == c)
    {
      *state = B85_S_HEADER;
      return true;
    }

    // Important, have to add B85_HEADER0 char to the hold.
    // NOTE: Assumes that B85_HEADER0 is in the alphabet.
    ctx->hold[ctx->pos++] = B85_G_DECODE[B85_HEADER0] - 1;
    *state = B85_S_NO_HEADER;
    return false;

  case B85_S_HEADER:
    if (B85_FOOTER0 == c)
    {
      *state = B85_S_FOOTER0;
      return true;
    }
    return false;

  case B85_S_FOOTER0:
    if (B85_FOOTER1 == c)
    {
      *state = B85_S_FOOTER;
      return true;
    }
    *state = B85_S_INVALID;
    return true;

  case B85_S_FOOTER:
  case B85_S_INVALID:
    break;
  }

  return true;
}

const char *
B85_DEBUG_ERROR_STRING (b85_result_t val)
{
  static const char *m[] = {
    "B85_E_OK",
    "B85_E_BAD_ALLOC",
    "B85_E_OVERFLOW",
    "B85_E_INVALID_CHAR",
    "B85_E_BAD_FOOTER",
    "B85_E_LOGIC_ERROR",
    "B85_E_API_MISUSE",
  };

  if (val >= 0 && val < dimof (m))
    return m[val];

  return "B85_E_UNSPECIFIED";
}

const char *
B85_ERROR_STRING (b85_result_t val)
{
  static const char *m[] = {
    "Success", // B85_E_OK
    "Bad Alloc", //B85_E_BAD_ALLOC
    "Byte sequence resulted in an overflow", //B85_E_OVERFLOW
    "Invalid character", // B85_E_INVALID_CHAR
    "Missing or invalid footer", // B85_E_BAD_FOOTER
    "Logic error", // B85_E_LOGIC_ERROR
    "API misuse", // B85_E_API_MISUSE
  };

  if (val >= 0 && val < dimof (m))
    return m[val];

  return "Unspecified error";
}

#if defined (B85_ZEROMQ)
#define B85_G_ENCODE g_z85_encode

/// ZeroMQ (Z85) alphabet.
static const uint8_t B85_G_ENCODE[] = {
  '0', '1', '2', '3', '4', '5', '6', '7',
  '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
  'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
  'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
  'w', 'x', 'y', 'z', 'A', 'B', 'C', 'D',
  'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L',
  'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T',
  'U', 'V', 'W', 'X', 'Y', 'Z', '.', '-',
  ':', '+', '=', '^', '!', '/', '*', '?',
  '&', '<', '>', '(', ')', '[', ']', '{',
  '}', '@', '%', '$', '#'
};

#else

#define B85_G_ENCODE g_ascii85_encode
/// Ascii85 alphabet.
static const uint8_t B85_G_ENCODE[] = {
  '!', '"', '#', '$', '%', '&', '\'', '(',
  ')', '*', '+', ',', '-', '.', '/', '0',
  '1', '2', '3', '4', '5', '6', '7', '8',
  '9', ':', ';', '<', '=', '>', '?', '@',
  'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
  'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
  'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
  'Y', 'Z', '[', '\\', ']', '^', '_', '`',
  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h',
  'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p',
  'q', 'r', 's', 't', 'u', 
};

#endif

/// Initializer for B85_G_DECODE (may be called multiple times).
static void
base85_decode_init (void)
{
  if (B85_G_DECODE[B85_G_ENCODE[0]])
    return;

  // NOTE: Assumes B85_G_DECODE[] was implicitly initialized with zeros.
  for (size_t i = 0; i < dimof (B85_G_ENCODE); ++i)
  {
    uint8_t c = B85_G_ENCODE[i];
    B85_G_DECODE[c] = i + 1;
  }
}

/// True if @a state is "critical", i.e. when whitespace is important.
static inline bool
base85_critical_state (b85_state_t state)
{
  return state == B85_S_HEADER0 || state == B85_S_FOOTER0;
}

/// Returns true if @a c is a white space character.
static bool
base85_whitespace (uint8_t c)
{
  switch (c)
  {
  case ' ':
  case '\n':
  case '\r':
  case '\t':
    return true;
  }
  return false;
}

static bool
base85_can_skip (uint8_t c, b85_state_t state)
{
  return !base85_critical_state (state) && base85_whitespace (c);
}

/// Returns the number of free bytes in the context's output buffer.
static ptrdiff_t
base85_context_bytes_remaining (struct base85_context_t *ctx)
{
  return (ctx->out + ctx->out_cb) - ctx->out_pos;
}

/// Increases the size of the context's output buffer.
static b85_result_t
base85_context_grow (struct base85_context_t *ctx)
{
  // How much additional memory to request if an allocation fails.
  static const size_t SMALL_DELTA = 256;

  // TODO: Refine size, and fallback strategy.
  size_t size = ctx->out_cb * 2;
  ptrdiff_t offset = ctx->out_pos - ctx->out;
  uint8_t *buffer = realloc (ctx->out, size);
  if (!buffer)
  {
    // Try a smaller allocation.
    buffer = realloc (ctx->out, ctx->out_cb + SMALL_DELTA);
    if (!buffer)
      return B85_E_BAD_ALLOC;
  }

  ctx->out = buffer;
  ctx->out_cb = size;
  ctx->out_pos = ctx->out + offset;
  return B85_E_OK;
}

/// Makes sure there is at least @a request bytes available in @a ctx.
static b85_result_t
base85_context_request_memory (struct base85_context_t *ctx, size_t request)
{
  if (base85_context_bytes_remaining (ctx) >= request)
    return B85_E_OK;

  return base85_context_grow (ctx);
}

uint8_t *
B85_GET_OUTPUT (struct base85_context_t *ctx, size_t *cb)
{
  if (!ctx || !cb)
    return NULL;

  *cb = ctx->out_pos - ctx->out;
  return ctx->out;
}

size_t
B85_GET_PROCESSED (struct base85_context_t *ctx)
{
  return ctx ? ctx->processed : 0;
}

void
B85_CLEAR_OUTPUT (struct base85_context_t *ctx)
{
  if (!ctx)
    return;

  ctx->out_pos = ctx->out;
}

b85_result_t
B85_CONTEXT_INIT (struct base85_context_t *ctx)
{
  static size_t INITIAL_BUFFER_SIZE = 1024;

  base85_decode_init ();

  if (!ctx)
    return B85_E_API_MISUSE;

  ctx->out_cb = 0;
  ctx->out_pos = NULL;
  ctx->processed = 0;
  ctx->pos = 0;
  ctx->state = B85_S_START;

  ctx->out = malloc (INITIAL_BUFFER_SIZE);
  if (!ctx->out)
    return B85_E_BAD_ALLOC;

  ctx->out_pos = ctx->out;
  ctx->out_cb = INITIAL_BUFFER_SIZE;
  return B85_E_OK;
}

void
B85_CONTEXT_RESET (struct base85_context_t *ctx)
{
  if (!ctx)
    return;

  // Do not reset out or out_cb.

  ctx->out_pos = ctx->out;
  ctx->processed = 0;
  ctx->pos = 0;
  ctx->state = B85_S_START;
}

void
B85_CONTEXT_DESTROY (struct base85_context_t *ctx)
{
  if (!ctx)
    return;

  uint8_t *out = ctx->out;
  ctx->out = NULL;
  ctx->out_pos = NULL;
  ctx->out_cb = 0;
  free (out);
}

static b85_result_t
base85_encode_strict (struct base85_context_t *ctx, bool no_short)
{
  uint8_t *h = ctx->hold;
  uint32_t v = h[0] << 24 | h[1] << 16 | h[2] << 8 | h[3];

  ctx->pos = 0;

  b85_result_t rv = B85_E_UNSPECIFIED;

#if !defined (B85_ZEROMQ)
  if (!v && !no_short)
  {
    rv = base85_context_request_memory (ctx, 1);
    if (rv)
      return rv;
    *ctx->out_pos = B85_ZERO_CHAR;
    ctx->out_pos++;
    return B85_E_OK;
  }
#endif

  rv = base85_context_request_memory (ctx, 5);
  if (rv)
    return rv;

  for (int c = 4; c >= 0; --c)
  {
    ctx->out_pos[c] = B85_G_ENCODE[v % 85];
    v /= 85;
  }
  ctx->out_pos += 5;
  return B85_E_OK;
}

b85_result_t
B85_ENCODE (const uint8_t *b, size_t cb_b, struct base85_context_t *ctx)
{
  if (!ctx || (cb_b && !b))
    return B85_E_API_MISUSE;

  if (!cb_b)
    return B85_E_OK;

  while (cb_b--)
  {
    ctx->hold[ctx->pos++] = *b++;
    ctx->processed++;
    if (4 == ctx->pos)
    {
      b85_result_t rv = base85_encode_strict (ctx, false);
      if (rv)
        return rv;
    }
  }

  return B85_E_OK;
}

b85_result_t
B85_ENCODE_LAST (struct base85_context_t *ctx)
{
  if (!ctx)
    return B85_E_API_MISUSE;

  b85_result_t rv = B85_E_UNSPECIFIED;
  size_t pos = ctx->pos;
  if (!pos)
  {
    rv = base85_context_request_memory (ctx, 1);
    if (B85_E_OK == rv)
      *ctx->out_pos = 0;
    return rv;
  }

  for (size_t i = pos; i < 4; ++i)
    ctx->hold[i] = 0;

  rv = base85_encode_strict (ctx, true);
  if (B85_E_OK == rv)
  {
    ctx->out_pos -= 4 - pos;
    *ctx->out_pos = 0;
  }
  return rv;
}

/// Decodes exactly 5 bytes from the decode context.
static b85_result_t
base85_decode_strict (struct base85_context_t *ctx)
{
  uint32_t v = 0;
  uint8_t *b = ctx->hold;

  b85_result_t rv = B85_E_UNSPECIFIED;
  rv = base85_context_request_memory (ctx, 4);
  if (rv)
    return rv;

  for (int c = 0; c < 4; ++c)
    v = v * 85 + b[c];

  // Check for overflow.
  if ((0xffffffff / 85 < v) || (0xffffffff - b[4] < (v *= 85)))
    return B85_E_OVERFLOW;

  v += b[4];

  for (int c = 24; c >= 0; c -= 8)
  {
    *(ctx->out_pos) = (v >> c) & 0xff;
    ctx->out_pos++;
  }

  ctx->pos = 0;
  return B85_E_OK;
}

b85_result_t
B85_DECODE (const uint8_t *b, size_t cb_b, struct base85_context_t *ctx)
{
  if (!ctx || (cb_b && !b))
    return B85_E_API_MISUSE;

  if (!cb_b)
    return B85_E_OK;

  b85_result_t rv = B85_E_UNSPECIFIED;
  while (cb_b--)
  {
    // Skip all input if a valid footer has already been found.
    if (B85_S_FOOTER == ctx->state)
      return B85_E_OK;

    if (B85_S_INVALID == ctx->state)
      return B85_E_BAD_FOOTER;

    uint8_t c = *b++;
    ctx->processed++;

    if (base85_can_skip (c, (b85_state_t) ctx->state))
      continue;

    if (base85_handle_state (c, ctx))
      continue;

#if !defined (B85_ZEROMQ)
    // Special case for 'z'.
    if (B85_ZERO_CHAR == c && !ctx->pos)
    {
      rv = base85_context_request_memory (ctx, 4);
      if (rv)
        return rv;

      memset (ctx->out_pos, 0, 4);
      ctx->out_pos += 4;
      continue;
    }
#endif

    uint8_t x = B85_G_DECODE[c];
    if (!x--)
      return B85_E_INVALID_CHAR;

    ctx->hold[ctx->pos++] = x;
    if (5 == ctx->pos)
    {
      rv = base85_decode_strict (ctx);
      if (rv)
        return rv;
    }
  }

  return B85_E_OK;
}

b85_result_t
B85_DECODE_LAST (struct base85_context_t *ctx)
{
  if (!ctx)
    return B85_E_API_MISUSE;

  if (B85_S_START == ctx->state)
    return B85_E_OK;

  if ((B85_S_FOOTER != ctx->state) && (B85_S_NO_HEADER != ctx->state))
    return B85_E_BAD_FOOTER;

  size_t pos = ctx->pos;

  if (!pos)
    return B85_E_OK;

  for (int i = pos; i < 5; ++i)
    ctx->hold[i] = B85_G_ENCODE[dimof (B85_G_ENCODE) - 1];

  b85_result_t rv = base85_decode_strict (ctx);
  if (rv)
    return rv;

  ctx->out_pos -= 5 - pos;
  return B85_E_OK;
}
