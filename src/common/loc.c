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

#include "shared/shared.h"
#include "common/cvar.h"
#include "common/files.h"
#include "common/common.h"

static cvar_t *loc_file;

#define npos ((size_t) -1)

#define MAX_LOC_KEY         64
#define MAX_LOC_FORMAT      1024
#define MAX_LOC_ARGS        8

typedef struct {
    uint8_t     arg_index;
    uint16_t    start, end;
} loc_arg_t;

typedef struct loc_string_s {
    char    key[MAX_LOC_KEY];
    char    format[MAX_LOC_FORMAT];

    size_t      num_arguments;
    loc_arg_t   arguments[MAX_LOC_ARGS];

    struct loc_string_s    *next;
} loc_string_t;

static loc_string_t *loc_head;

size_t Loc_Localize(const char *base, const char **arguments, size_t num_arguments, char *output, size_t output_length)
{
    // skip $ prefix if it exists
    if (*base == '$') {
        base++;
    }

    // find loc
    const loc_string_t *str = loc_head;

    for (; str != NULL; str = str->next) {
        if (!strcmp(str->key, base)) {
            break;
        }
    }

    if (!str) {
        return Q_strlcpy(output, base, output_length);
    }

    // easy case
    if (!str->num_arguments) {
        return Q_strlcpy(output, str->format, output_length);
    }

    // check args
    for (size_t i = 0; i < str->num_arguments; i++) {
        if (str->arguments[i].arg_index >= num_arguments) {
            Com_WPrintf("%s: base \"%s\" localized with too few arguments\n", __func__, base);
            return Q_strlcpy(output, base, output_length);
        }
    }
    
    for (size_t i = 0; i < num_arguments; i++) {
        if (!arguments[i]) {
            Com_WPrintf("%s: invalid argument at position %u\n", __func__, i);
            return Q_strlcpy(output, base, output_length);
        }
    }

    // fill prefix if we have one
    loc_arg_t *arg = &str->arguments[0];

    Q_strnlcpy(output, str->format, arg->start, output_length);

    static char localized_arg[MAX_STRING_CHARS];

    for (size_t i = 0; i < str->num_arguments - 1; i++) {

        Loc_Localize(arguments[arg->arg_index], NULL, 0, localized_arg, sizeof(localized_arg));
        Q_strlcat(output, localized_arg, output_length);

        loc_arg_t *next_arg = &str->arguments[i + 1];

        Q_strnlcat(output, str->format + arg->end, next_arg->start - arg->end, output_length);

        arg = next_arg;
    }
    
    Loc_Localize(arguments[arg->arg_index], NULL, 0, localized_arg, sizeof(localized_arg));
    Q_strlcat(output, localized_arg, output_length);

    return Q_strlcat(output, str->format + arg->end, output_length);
}

static size_t find_start_of_utf8_codepoint(const char *str, size_t str_length, size_t pos)
{
    if (pos >= str_length) {
        return npos;
    }

    for (ptrdiff_t i = pos; i >= 0; i--) {
        char ch = str[i];

        if ((ch & 0x80) == 0) {
            return i;
        } else if ((ch & 0xC0) == 0x80) {
            continue;
        } else {
            return i;
        }
    }

    return npos;
}

static size_t find_end_of_utf8_codepoint(const char *str, size_t str_length, size_t pos)
{
    if (pos >= str_length) {
        return npos;
    }

    for (size_t i = pos; i < str_length; i++) {
        char ch = str[i];

        if ((ch & 0x80) == 0) {
            return i;
        } else if((ch & 0xC0) == 0x80) {
            continue;
        } else {
            return i;
        }
    }

    return str_length;
}

static int loccmpfnc(const void *_a, const void *_b)
{
    const loc_arg_t *a = (const loc_arg_t *)_a;
    const loc_arg_t *b = (const loc_arg_t *)_b;
    return a->start - b->start;
}

static void Loc_Unload()
{
    if (!loc_head) {
        return;
    }

    for (loc_string_t *rover = loc_head; rover; ) {
        loc_string_t *next = rover->next;
        Z_Free(rover);
        rover = next;
    }

    loc_head = NULL;
}

/*
================
Loc_ReloadFile
================
*/
void Loc_ReloadFile()
{
    Loc_Unload();

	char *buffer;

	int64_t len = FS_LoadFile(loc_file->string, &buffer);

	if (!buffer) {
		return;
	}

    size_t line_start = 0;
    size_t num_locs = 0;
    size_t line_num = 0;

    loc_string_t **tail = &loc_head;

    while (true) {
        // find the end of the current line; might be EOF
        size_t line_end = line_start;

        while (true) {
            if (!buffer[line_end] || buffer[line_end] == '\n') {
                break;
            }
            
            line_end = find_end_of_utf8_codepoint(buffer, len, line_end);

            if (line_end == npos) {
                break;
            }

            line_end++;
        }

        if (line_end == npos) {
            break;
        }

        char *line_ptr = buffer + line_start;

        // cull whitespace from the end
        {
            size_t ws = line_end;

            while (true) {
                if (ws <= line_start || ws == npos || !Q_isspace(buffer[ws])) {
                    break;
                }
                
                ws = find_start_of_utf8_codepoint(buffer, len, ws);

                if (ws <= line_start || ws == npos) {
                    break;
                }

                ws--;
            }


            // we are an entirely-whitespace line most likely
            if (ws == npos || ws == line_start) {
                goto skip_line;
            }

            for (ws++; ws <= line_end; ws++) {
                buffer[ws] = '\0';
            }
        }

        // parse the key, luckily it's guaranteed to be ASCII
        char *parse_ptr = line_ptr;

        const char *token = COM_Parse(&parse_ptr);

        // bad token?
        if (!*token) {
            goto skip_line;
        }

        // TODO skip these for now
        if (parse_ptr[0] && parse_ptr[1] == '<') {
            goto skip_line;
        }

        loc_string_t loc; 

        Q_strlcpy(loc.key, token, sizeof(loc.key));
        loc.format[0] = '\0';

        if (strncmp(parse_ptr, " = \"", 4)) {
            goto skip_line;
        }

        parse_ptr += 4;

        size_t quote_end = (parse_ptr - buffer);

        // find the closing quote
        while (true) {
            if (quote_end == npos || !buffer[quote_end] || quote_end >= len) {
                break;
            }
            
            quote_end = find_end_of_utf8_codepoint(buffer, len, quote_end);

            if (quote_end == npos) {
                Com_SetLastError("EOF before quote end found");
                goto skip_line;
            }

            quote_end++;

            // skip escape sequences
            if (buffer[quote_end] == '\\') {
                quote_end = find_end_of_utf8_codepoint(buffer, len, quote_end);

                if (quote_end == npos) {
                    Com_SetLastError("EOF before quote end found");
                    goto skip_line;
                }

                quote_end++;
            } else if (buffer[quote_end] == '\"') {
                break;
            }
        }

        if (quote_end >= len || quote_end == npos) {
            Com_SetLastError("EOF before quote end found");
            goto skip_line;
        }

        buffer[quote_end] = '\0';

        size_t parse_len = &buffer[quote_end] - line_ptr;

        // handle escape sequences
        size_t fmt_offset = 0;

        while (true) {

            if (!parse_ptr[fmt_offset]) {
                break;
            }

            if (parse_ptr[fmt_offset] == '\\') {

                fmt_offset = find_end_of_utf8_codepoint(parse_ptr, parse_len, fmt_offset);

                if (fmt_offset == npos) {
                    break;
                }

                fmt_offset++;
                
                bool eaten = false;

                if (parse_ptr[fmt_offset] == 'n') {
                    Q_strlcat(loc.format, "\n", sizeof(loc.format));
                    eaten = true;
                }

                if (eaten) {
                    
                    fmt_offset = find_end_of_utf8_codepoint(parse_ptr, parse_len, fmt_offset);
                    
                    if (fmt_offset == npos) {
                        break;
                    }

                    fmt_offset++;
                    continue;
                }
            }

            // copy entire codepoint
            size_t next_offset = find_end_of_utf8_codepoint(parse_ptr, parse_len, fmt_offset);

            if (next_offset == npos) {
                break;
            }

            Q_strnlcat(loc.format, parse_ptr + fmt_offset, (next_offset - fmt_offset) + 1, sizeof(loc.format));

            fmt_offset = next_offset + 1;
        }

        // if -1, a positional argument was encountered
        int32_t arg_index = 0;

        loc.num_arguments = 0;
        loc.next = NULL;

        parse_ptr = loc.format;

        size_t arg_offset = 0;

        // parse out arguments
        size_t arg_rover = 0;

        while (true) {
            if (arg_rover >= sizeof(loc.format) || !loc.format[arg_rover]) {
                break;
            }

            if (loc.format[arg_rover] == '{') {
                size_t arg_start = arg_rover;

                arg_rover = find_end_of_utf8_codepoint(loc.format, sizeof(loc.format), arg_rover) + 1;

                if (loc.format[arg_rover] && loc.format[arg_rover] == '{') {
                    continue; // escape sequence
                }

                // argument encountered
                if (loc.num_arguments == MAX_LOC_ARGS) {
                    Com_SetLastError("too many arguments");
                    goto line_error; // no more room
                }

                loc_arg_t *arg = &loc.arguments[loc.num_arguments++];

                arg->start = arg_start - arg_offset;

                // check if we have a numerical value
                char *end_ptr;
                arg->arg_index = strtol(&loc.format[arg_rover], &end_ptr, 10);

                if (end_ptr == &loc.format[arg_rover]) {

                    if (arg_index == -1) {
                        Com_SetLastError("encountered sequential argument, but has positional args");
                        goto line_error;
                    }

                    // sequential
                    arg->arg_index = arg_index++;
                } else {

                    // positional
                    if (arg_index > 0) {
                        Com_SetLastError("encountered positional argument, but has sequential args");
                        goto line_error;
                    }

                    // mark us off so we can't find sequentials
                    arg_index = -1;
                }

                // find the end of this argument
                arg_rover = (end_ptr - loc.format) - 1;

                while (true) {
                    if (arg_rover >= sizeof(loc.format) || !loc.format[arg_rover]) {
                        break;
                    }
                
                    arg_rover = find_end_of_utf8_codepoint(loc.format, sizeof(loc.format), arg_rover);

                    if (arg_rover == npos) {
                        Com_SetLastError("EOF before end of argument found");
                        goto line_error;
                    }

                    arg_rover++;
                
                    if (loc.format[arg_rover] != '}') {
                        continue;
                    }

                    size_t arg_end = arg_rover;

                    arg_rover = find_end_of_utf8_codepoint(loc.format, sizeof(loc.format), arg_rover) + 1;

                    if (loc.format[arg_rover] && loc.format[arg_rover] == '}') {
                        continue; // escape sequence
                    }

                    // we found it
                    arg->end = (arg_end - arg_offset) + 1;
                    break;
                }
            }

            arg_rover = find_end_of_utf8_codepoint(loc.format, sizeof(loc.format), arg_rover);

            if (arg_rover == npos) {
                break;
            }

            arg_rover++;
        }

        if (loc.num_arguments) {
            // sort the arguments by start position
            qsort(loc.arguments, loc.num_arguments, sizeof(loc_arg_t), loccmpfnc);
        }

        // link us in and copy off
        *tail = Z_Malloc(sizeof(loc_string_t));
        memcpy(*tail, &loc, sizeof(loc));
        tail = &((*tail)->next);

        num_locs++;
        goto skip_line;

line_error:
        Com_WPrintf("[%s:%i]: %s\n", loc_file->string, line_num, Com_GetLastError());

skip_line:
        line_start = line_end + 1;
        line_num++;
    }

	FS_FreeFile(buffer);

    Com_Printf("Loaded %u localization strings\n", num_locs);
}

/*
================
Loc_Init
================
*/
void Loc_Init()
{
	loc_file = Cvar_Get("loc_file", "localization/loc_english.txt", 0);

	Loc_ReloadFile();
}