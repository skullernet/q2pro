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
#include "common/cmd.h"
#include "common/common.h"
#include "common/cvar.h"
#include "common/files.h"
#if USE_REF
#include "client/video.h"
#endif
#include "system/system.h"
#include "tty.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>

#if USE_SDL
#include <SDL.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#define exit(s) emscripten_force_exit(s)
#endif

cvar_t  *sys_basedir;
cvar_t  *sys_libdir;
cvar_t  *sys_homedir;

#if USE_SYSCON
extern cvar_t   *console_prefix;
#endif

static int terminate;
static bool flush_logs;

/*
===============================================================================

GENERAL ROUTINES

===============================================================================
*/

void Sys_DebugBreak(void)
{
    raise(SIGTRAP);
}

unsigned Sys_Milliseconds(void)
{
    struct timespec ts;
    (void)clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000UL + ts.tv_nsec / 1000000UL;
}

/*
=================
Sys_Quit

This function never returns.
=================
*/
void Sys_Quit(void)
{
    tty_shutdown_input();
#if USE_SDL
    SDL_Quit();
#endif
    exit(EXIT_SUCCESS);
}

#define SYS_SITE_CFG    "/etc/default/q2pro"

void Sys_AddDefaultConfig(void)
{
    FILE *fp;
    size_t len;

    fp = fopen(SYS_SITE_CFG, "r");
    if (!fp) {
        return;
    }

    len = fread(cmd_buffer.text, 1, cmd_buffer.maxsize - 1, fp);
    fclose(fp);

    cmd_buffer.text[len] = 0;
    cmd_buffer.cursize = COM_Compress(cmd_buffer.text);
    if (cmd_buffer.cursize) {
        Com_Printf("Execing %s\n", SYS_SITE_CFG);
        Cbuf_Execute(&cmd_buffer);
    }
}

void Sys_Sleep(int msec)
{
    struct timespec req = {
        .tv_sec = msec / 1000,
        .tv_nsec = (msec % 1000) * 1000000
    };
    nanosleep(&req, NULL);
}

const char *Sys_ErrorString(int err)
{
    return strerror(err);
}

#if USE_AC_CLIENT
bool Sys_GetAntiCheatAPI(void)
{
    Sys_Sleep(1500);
    return false;
}
#endif

bool Sys_SetNonBlock(int fd, bool nb)
{
    int ret = fcntl(fd, F_GETFL, 0);
    if (ret == -1)
        return false;
    if ((bool)(ret & O_NONBLOCK) == nb)
        return true;
    return fcntl(fd, F_SETFL, ret ^ O_NONBLOCK) == 0;
}

static void usr1_handler(int signum)
{
    flush_logs = true;
}

static void term_handler(int signum)
{
    terminate = signum;
}

/*
=================
Sys_Init
=================
*/
void Sys_Init(void)
{
    const char *homedir;

    signal(SIGTERM, term_handler);
    signal(SIGINT, term_handler);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGHUP, term_handler);
    signal(SIGUSR1, usr1_handler);

    // basedir <path>
    // allows the game to run from outside the data tree
    sys_basedir = Cvar_Get("basedir", DATADIR, CVAR_NOSET);

    // homedir <path>
    // specifies per-user writable directory for demos, screenshots, etc
    if (HOMEDIR[0] == '~') {
        char *s = getenv("HOME");
        if (s && strlen(s) >= MAX_OSPATH - MAX_QPATH)
            Sys_Error("HOME path too long");
        if (s && *s) {
            homedir = va("%s%s", s, &HOMEDIR[1]);
        } else {
            homedir = "";
        }
    } else {
        homedir = HOMEDIR;
    }

    sys_homedir = Cvar_Get("homedir", homedir, CVAR_NOSET);
    sys_libdir = Cvar_Get("libdir", LIBDIR, CVAR_NOSET);

    tty_init_input();
}

/*
=================
Sys_Error
=================
*/
void Sys_Error(const char *error, ...)
{
    va_list     argptr;
    char        text[MAXERRORMSG];
    const char  *pre = "";

    tty_shutdown_input();

#if USE_REF
    if (vid && vid->fatal_shutdown)
        vid->fatal_shutdown();
#endif

    va_start(argptr, error);
    Q_vsnprintf(text, sizeof(text), error, argptr);
    va_end(argptr);

#if USE_SYSCON
    if (console_prefix && !strncmp(console_prefix->string, "<?>", 3))
        pre = "<3>";
#endif

    fprintf(stderr,
            "%s********************\n"
            "%sFATAL: %s\n"
            "%s********************\n", pre, pre, text, pre);
    exit(EXIT_FAILURE);
}

/*
========================================================================

DLL LOADING

========================================================================
*/

/*
=================
Sys_FreeLibrary
=================
*/
void Sys_FreeLibrary(void *handle)
{
    if (handle && dlclose(handle)) {
        Com_Error(ERR_FATAL, "dlclose failed on %p: %s", handle, dlerror());
    }
}

/*
=================
Sys_LoadLibrary
=================
*/
void *Sys_LoadLibrary(const char *path, const char *sym, void **handle)
{
    void    *module, *entry;

    *handle = NULL;

    dlerror();
    module = dlopen(path, RTLD_LAZY);
    if (!module) {
        Com_SetLastError(dlerror());
        return NULL;
    }

    if (sym) {
        dlerror();
        entry = dlsym(module, sym);
        if (!entry) {
            Com_SetLastError(dlerror());
            dlclose(module);
            return NULL;
        }
    } else {
        entry = NULL;
    }

    *handle = module;
    return entry;
}

void *Sys_GetProcAddress(void *handle, const char *sym)
{
    void    *entry;

    dlerror();
    entry = dlsym(handle, sym);
    if (!entry)
        Com_SetLastError(dlerror());

    return entry;
}

/*
===============================================================================

MISC

===============================================================================
*/

/*
=================
Sys_ListFiles_r
=================
*/
void Sys_ListFiles_r(listfiles_t *list, const char *path, int depth)
{
    struct dirent *ent;
    DIR *dir;
    struct stat st;
    char fullpath[MAX_OSPATH];
    char *name;
    void *info;

    if (list->count >= MAX_LISTED_FILES) {
        return;
    }

    if ((dir = opendir(path)) == NULL) {
        return;
    }

    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') {
            continue; // ignore dotfiles
        }

        if (Q_concat(fullpath, sizeof(fullpath), path, "/",
                     ent->d_name) >= sizeof(fullpath)) {
            continue;
        }

        st.st_mode = 0;

#ifdef _DIRENT_HAVE_D_TYPE
        // try to avoid stat() if possible
        if (!(list->flags & FS_SEARCH_EXTRAINFO)
            && ent->d_type != DT_UNKNOWN
            && ent->d_type != DT_LNK) {
            st.st_mode = DTTOIF(ent->d_type);
        }
#endif

        if (st.st_mode == 0 && stat(fullpath, &st) == -1) {
            continue;
        }

        // pattern search implies recursive search
        if ((list->flags & (FS_SEARCH_BYFILTER | FS_SEARCH_RECURSIVE))
            && S_ISDIR(st.st_mode) && depth < MAX_LISTED_DEPTH) {
            Sys_ListFiles_r(list, fullpath, depth + 1);

            // re-check count
            if (list->count >= MAX_LISTED_FILES) {
                break;
            }
        }

        // check type
        if (list->flags & FS_SEARCH_DIRSONLY) {
            if (!S_ISDIR(st.st_mode)) {
                continue;
            }
        } else {
            if (!S_ISREG(st.st_mode)) {
                continue;
            }
        }

        // check filter
        if (list->filter) {
            if (list->flags & FS_SEARCH_BYFILTER) {
                if (!FS_WildCmp(list->filter, fullpath + list->baselen)) {
                    continue;
                }
            } else {
                if (!FS_ExtCmp(list->filter, ent->d_name)) {
                    continue;
                }
            }
        }

        // skip path
        name = fullpath + list->baselen;

        // strip extension
        if (list->flags & FS_SEARCH_STRIPEXT) {
            *COM_FileExtension(name) = 0;

            if (!*name) {
                continue;
            }
        }

        // copy info off
        if (list->flags & FS_SEARCH_EXTRAINFO) {
            info = FS_CopyInfo(name, st.st_size, st.st_ctime, st.st_mtime);
        } else {
            info = FS_CopyString(name);
        }

        list->files = FS_ReallocList(list->files, list->count + 1);
        list->files[list->count++] = info;

        if (list->count >= MAX_LISTED_FILES) {
            break;
        }
    }

    closedir(dir);
}

/*
=================
main
=================
*/
int main(int argc, char **argv)
{
    if (argc > 1) {
        if (!strcmp(argv[1], "-v") || !strcmp(argv[1], "--version")) {
            fprintf(stderr, "%s\n", com_version_string);
            return EXIT_SUCCESS;
        }
        if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) {
            fprintf(stderr, "Usage: %s [+command arguments] [...]\n", argv[0]);
            return EXIT_SUCCESS;
        }
    }

#ifndef __EMSCRIPTEN__
    if (!getuid() || !geteuid()) {
        fprintf(stderr, "You can not run " PRODUCT " as superuser "
                "for security reasons!\n");
        return EXIT_FAILURE;
    }
#endif

    Qcommon_Init(argc, argv);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(Qcommon_Frame, 0, true);
#else
    while (!terminate) {
        if (flush_logs) {
            Com_FlushLogs();
            flush_logs = false;
        }
        Qcommon_Frame();
    }
#endif

    Com_Printf("%s\n", strsignal(terminate));
    Com_Quit(NULL, ERR_DISCONNECT);

    return EXIT_FAILURE; // never gets here
}
