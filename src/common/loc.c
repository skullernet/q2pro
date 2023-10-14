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

#define MAX_LOC_KEY         64
#define MAX_LOC_FORMAT      1024
#define MAX_LOC_ARGS        8

// must be POT
#define LOC_HASH_SIZE        256

typedef struct {
    uint8_t     arg_index;
    uint16_t    start, end;
} loc_arg_t;

typedef struct loc_string_s {
    char    key[MAX_LOC_KEY];
    char    format[MAX_LOC_FORMAT];

    size_t      num_arguments;
    loc_arg_t   arguments[MAX_LOC_ARGS];

    struct loc_string_s    *next, *hash_next;
} loc_string_t;

static loc_string_t *loc_head;
static loc_string_t *loc_hash[LOC_HASH_SIZE];

static int loccmpfnc(const void *_a, const void *_b)
{
    const loc_arg_t *a = (const loc_arg_t *)_a;
    const loc_arg_t *b = (const loc_arg_t *)_b;
    return a->start - b->start;
}

/*
================
Loc_Parse
================
*/
static bool Loc_Parse(loc_string_t *loc)
{
    // if -1, a positional argument was encountered
    int32_t arg_index = 0;

    // parse out arguments
    size_t arg_rover = 0;

    while (true) {
        if (arg_rover >= sizeof(loc->format) || !loc->format[arg_rover]) {
            break;
        }

        if (loc->format[arg_rover] == '{') {
            size_t arg_start = arg_rover;

            arg_rover++;

            if (loc->format[arg_rover] && loc->format[arg_rover] == '{') {
                continue; // escape sequence
            }

            // argument encountered
            if (loc->num_arguments == MAX_LOC_ARGS) {
                Com_SetLastError("too many arguments");
                return false;
            }

            loc_arg_t *arg = &loc->arguments[loc->num_arguments++];

            arg->start = arg_start;

            // check if we have a numerical value
            char *end_ptr;
            arg->arg_index = strtol(&loc->format[arg_rover], &end_ptr, 10);

            if (end_ptr == &loc->format[arg_rover]) {

                if (arg_index == -1) {
                    Com_SetLastError("encountered sequential argument, but has positional args");
                    return false;
                }

                // sequential
                arg->arg_index = arg_index++;
            } else {

                // positional
                if (arg_index > 0) {
                    Com_SetLastError("encountered positional argument, but has sequential args");
                    return false;
                }

                // mark us off so we can't find sequentials
                arg_index = -1;
            }

            // find the end of this argument
            arg_rover = (end_ptr - loc->format) - 1;

            while (true) {

                if (arg_rover >= sizeof(loc->format) || !loc->format[arg_rover]) {
                    Com_SetLastError("EOF before end of argument found");
                    return false;
                }

                arg_rover++;
                
                if (loc->format[arg_rover] != '}') {
                    continue;
                }

                size_t arg_end = arg_rover;

                arg_rover++;

                if (loc->format[arg_rover] && loc->format[arg_rover] == '}') {
                    continue; // escape sequence
                }

                // we found it
                arg->end = arg_end + 1;
                break;
            }
        }

        arg_rover++;
    }

    if (loc->num_arguments) {
        // sort the arguments by start position
        qsort(loc->arguments, loc->num_arguments, sizeof(loc_arg_t), loccmpfnc);
    }

    return true;
}

// just as best guess as to whether the given in-place string
// has arguments to parse or not. fmt uses {{ as escape, so
// we can assume any { that isn't followed by a { means we
// have an arg.
static bool Loc_HasArguments(const char *base)
{
    for (const char *rover = base; *rover; rover++) {
        if (*rover == '{') {
            rover++;

            if (!*rover) {
                return false;
            } else if (*rover != '{') {
                return true;
            }
        }
    }

    return false;
}

// find the given loc_string_t from the hashed list of localized
// strings
static const loc_string_t *Loc_Find(const char *base)
{
    // find loc via hash
    uint32_t hash = Com_HashString(base, LOC_HASH_SIZE) & (LOC_HASH_SIZE - 1);

    if (!loc_hash[hash]) {
        return NULL;
    }

    for (const loc_string_t *str = loc_hash[hash]; str != NULL; str = str->hash_next) {
        if (!strcmp(str->key, base)) {
            return str;
        }
    }

    return NULL;
}

size_t Loc_Localize(const char *base, bool allow_in_place, const char **arguments, size_t num_arguments, char *output, size_t output_length)
{
    Q_assert(base);

    static loc_string_t in_place_loc;
    const loc_string_t *str;

    // re-release supports two types of localizations - ones in
    // the loc file (prefixed with $) and in-place localizations
    // that are formatted at runtime.
    if (!allow_in_place) {
        if (*base != '$') {
            return Q_strlcpy(output, base, output_length);
        }

        base++;
        str = Loc_Find(base);
    } else {
        if (*base == '$') {
            base++;
            str = Loc_Find(base);
        } else if (Loc_HasArguments(base)) {
            in_place_loc.num_arguments = 0;
            Q_strlcpy(in_place_loc.format, base, sizeof(in_place_loc.format));

            if (!Loc_Parse(&in_place_loc)) {
                Com_WPrintf("in-place localization of \"%s\" failed: %s\n", base, Com_GetLastError());
                return Q_strlcpy(output, base, output_length);
            }

            str = &in_place_loc;
        } else {
            return Q_strlcpy(output, base, output_length);
        }
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
    const loc_arg_t *arg = &str->arguments[0];

    Q_strnlcpy(output, str->format, arg->start, output_length);

    static char localized_arg[MAX_STRING_CHARS];

    for (size_t i = 0; i < str->num_arguments - 1; i++) {

        Loc_Localize(arguments[arg->arg_index], false, NULL, 0, localized_arg, sizeof(localized_arg));
        Q_strlcat(output, localized_arg, output_length);

        const loc_arg_t *next_arg = &str->arguments[i + 1];

        Q_strnlcat(output, str->format + arg->end, next_arg->start - arg->end, output_length);

        arg = next_arg;
    }
    
    Loc_Localize(arguments[arg->arg_index], false, NULL, 0, localized_arg, sizeof(localized_arg));
    Q_strlcat(output, localized_arg, output_length);

    return Q_strlcat(output, str->format + arg->end, output_length);
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
    memset(loc_hash, 0, sizeof(loc_hash));
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

    const char *parse_buf = buffer;

    while (true) {
        loc_string_t loc; 

        char *key = COM_ParseEx(&parse_buf, 0, loc.key, sizeof(loc.key));

        if (!*key) {
            break;
        }

        const char *equals = COM_Parse(&parse_buf);
        bool has_platform_spec = false;

        // check for console specs
        if (!*equals) {
            break;
        } else if (*equals == '<') {
            has_platform_spec = true;

            // skip these for now
            while (*equals && equals[strlen(equals) - 1] != '>') {
                equals = COM_Parse(&parse_buf);
            }

            equals = COM_Parse(&parse_buf);
        }

        // syntax error
        if (strcmp(equals, "=")) {
            break;
        }

        char *value = COM_ParseEx(&parse_buf, PARSE_FLAG_ESCAPE, loc.format, sizeof(loc.format));

        // skip platform specifiers
        if (has_platform_spec) {
            continue;
        }

        loc.num_arguments = 0;
        loc.next = loc.hash_next = NULL;

        if (!Loc_Parse(&loc)) {
            goto line_error;
        }

        // link us in and copy off
        *tail = Z_Malloc(sizeof(loc_string_t));
        memcpy(*tail, &loc, sizeof(loc));

        // hash
        uint32_t hash = Com_HashString(loc.key, LOC_HASH_SIZE) & (LOC_HASH_SIZE - 1);
        if (!loc_hash[hash]) {
            loc_hash[hash] = *tail;
        } else {
            (*tail)->hash_next = loc_hash[hash];
            loc_hash[hash] = *tail;
        }

        tail = &((*tail)->next);

        num_locs++;
        continue;

line_error:
        Com_WPrintf("%s (%s): %s\n", loc_file->string, key, Com_GetLastError());
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