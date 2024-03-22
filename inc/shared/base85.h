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

#if !defined (BASE85_H__INCLUDED__)
#define BASE85_H__INCLUDED__

#include <stddef.h>
#include <stdint.h>

#if defined (B85_ZEROMQ)
#define B85_NAME(name) z85_##name
#else
#define B85_NAME(name) ascii85_##name
#endif

#define B85_DEBUG_ERROR_STRING B85_NAME (debug_error_string)
#define B85_ERROR_STRING B85_NAME (error_string)
#define B85_GET_OUTPUT B85_NAME (get_output)
#define B85_GET_PROCESSED B85_NAME (get_processed)
#define B85_CLEAR_OUTPUT B85_NAME (clear_output)
#define B85_CONTEXT_INIT B85_NAME (context_init)
#define B85_CONTEXT_RESET B85_NAME (context_reset)
#define B85_CONTEXT_DESTROY B85_NAME (context_destroy)
#define B85_ENCODE B85_NAME (encode)
#define B85_ENCODE_LAST B85_NAME (encode_last)
#define B85_DECODE B85_NAME (decode)
#define B85_DECODE_LAST B85_NAME (decode_last)

/// Base85 result values.
typedef enum
{
  /// Unspecified error. This value is not used by the library directly. It can
  /// be used by clients that wish to pass through b85_result_t values.
  B85_E_UNSPECIFIED = -1,

  /// Success.
  B85_E_OK = 0,

  /// Memory allocation failure.
  B85_E_BAD_ALLOC,

  /// Decoding a byte sequence resulted in an integer overflow.
  B85_E_OVERFLOW,

  /// An invalid byte was encountered (decoding).
  B85_E_INVALID_CHAR,

  /// The ascii85 footer is missing, or a rogue '~' byte was encountered.
  B85_E_BAD_FOOTER,

  /// Logic error in the library implementation.
  B85_E_LOGIC_ERROR,

  /// Indicates API misuse by a client.
  B85_E_API_MISUSE,

  /// End marker
  B85_E_END
} b85_result_t;

/// Tranlates @a val to a debug error string (i.e., "B85_E_OK").
const char *
B85_DEBUG_ERROR_STRING (b85_result_t val);

/// Translates @a val to an error string.
const char *
B85_ERROR_STRING (b85_result_t val);

/// Context for the base85 decode functions.
struct base85_context_t
{
  /// Bytes "on deck" for encoding/decoding. Unsigned is important.
  uint8_t hold[5];

  /// The current hold position (i.e. how many bytes are currently in hold).
  size_t pos;

  /// Output buffer, memory is managed by the encode/decode functions.
  uint8_t *out;

  /// Current output position.
  uint8_t *out_pos;

  /// Number of bytes allocated for the out buffer.
  size_t out_cb;

  /// The total number of bytes processed as input.
  size_t processed;

  /// Internal state (used for keeping track of the header/footer during
  /// decoding).
  uint8_t state;
};

/// Gets the output from @a ctx.
/// Returns the number of available bytes in @a cb.
/// @pre @a ctx is valid.
uint8_t *
B85_GET_OUTPUT (struct base85_context_t *ctx, size_t *cb);

/// Gets the number of input bytes processed by @a ctx.
size_t
B85_GET_PROCESSED (struct base85_context_t *ctx);

/// Clears the output buffer in @a ctx. i.e. the next call to
/// B85_GET_OUTPUT() will return a byte count of zero.
/// @pre @a ctx is valid.
void
B85_CLEAR_OUTPUT (struct base85_context_t *ctx);

/// Initializes a context object.
/// When done with the context, call B85_CONTEXT_DESTROY().
b85_result_t
B85_CONTEXT_INIT (struct base85_context_t *ctx);

/// Resets an existing context, but does not free its memory. This is useful
/// for resetting the context before encoding/decoding a new data stream.
void
B85_CONTEXT_RESET (struct base85_context_t *ctx);

/// Context cleanup. Frees memory associated with the context.
void
B85_CONTEXT_DESTROY (struct base85_context_t *ctx);

/// Encodes @a cb_b bytes from @a b, and stores the result in @a ctx.
/// @pre @a b must contain at least @a cb_b bytes, and @a ctx must be a valid
/// context. 
///
/// Note: B85_ENCODE_LAST() must be called in order to finalize the encode
/// operation.
b85_result_t
B85_ENCODE (const uint8_t *b, size_t cb_b, struct base85_context_t *ctx);

/// Finalizes an encode operation that was initiated by calling base85_encode().
/// @pre @a ctx must be valid.
b85_result_t
B85_ENCODE_LAST (struct base85_context_t *ctx);

/// Decodes @a cb_b bytes from @a b. The result is stored in @a ctx.
/// @pre @a b must contain at least @a cb_b bytes, and @a ctx must be a valid
/// context.
///
/// Note: B85_DECODE_LAST() must be called in order to finalize the decode
/// operation.
///
/// @return 0 for success.
b85_result_t
B85_DECODE (
  const uint8_t *b, size_t cb_b, struct base85_context_t *ctx
);

/// Finalizes a decode operation that was initiated by calling B85_DECODE().
/// @pre @a ctx must be valid.
///
/// @return 0 for success.
b85_result_t
B85_DECODE_LAST (struct base85_context_t *ctx);

#endif // !defined (BASE85_H__INCLUDED__)
