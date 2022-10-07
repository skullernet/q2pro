/*
Copyright (C) 2022 Andrey Nazarov

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "shared/shared.h"
#include "client/keys.h"
#include "keytables.h"

// matches keycodes from xf86-input-keyboard/src/atKeynames.h
static const uint8_t keys[] = {
    [  1] = K_ESCAPE,
    [  2] = '1',
    [  3] = '2',
    [  4] = '3',
    [  5] = '4',
    [  6] = '5',
    [  7] = '6',
    [  8] = '7',
    [  9] = '8',
    [ 10] = '9',
    [ 11] = '0',
    [ 12] = '-',
    [ 13] = '=',
    [ 14] = K_BACKSPACE,
    [ 15] = K_TAB,
    [ 16] = 'q',
    [ 17] = 'w',
    [ 18] = 'e',
    [ 19] = 'r',
    [ 20] = 't',
    [ 21] = 'y',
    [ 22] = 'u',
    [ 23] = 'i',
    [ 24] = 'o',
    [ 25] = 'p',
    [ 26] = '[',
    [ 27] = ']',
    [ 28] = K_ENTER,
    [ 29] = K_LCTRL,
    [ 30] = 'a',
    [ 31] = 's',
    [ 32] = 'd',
    [ 33] = 'f',
    [ 34] = 'g',
    [ 35] = 'h',
    [ 36] = 'j',
    [ 37] = 'k',
    [ 38] = 'l',
    [ 39] = ';',
    [ 40] = '\'',
    [ 41] = '`',
    [ 42] = K_LSHIFT,
    [ 43] = '\\',
    [ 44] = 'z',
    [ 45] = 'x',
    [ 46] = 'c',
    [ 47] = 'v',
    [ 48] = 'b',
    [ 49] = 'n',
    [ 50] = 'm',
    [ 51] = ',',
    [ 52] = '.',
    [ 53] = '/',
    [ 54] = K_RSHIFT,
    [ 56] = K_LALT,
    [ 57] = K_SPACE,
    [ 58] = K_CAPSLOCK,
    [ 59] = K_F1,
    [ 60] = K_F2,
    [ 61] = K_F3,
    [ 62] = K_F4,
    [ 63] = K_F5,
    [ 64] = K_F6,
    [ 65] = K_F7,
    [ 66] = K_F8,
    [ 67] = K_F9,
    [ 68] = K_F10,
    [ 69] = K_NUMLOCK,
    [ 70] = K_SCROLLOCK,
    [ 71] = K_KP_HOME,
    [ 72] = K_KP_UPARROW,
    [ 73] = K_KP_PGUP,
    [ 74] = K_KP_MINUS,
    [ 75] = K_KP_LEFTARROW,
    [ 76] = K_KP_5,
    [ 77] = K_KP_RIGHTARROW,
    [ 78] = K_KP_PLUS,
    [ 79] = K_KP_END,
    [ 80] = K_KP_DOWNARROW,
    [ 81] = K_KP_PGDN,
    [ 82] = K_KP_INS,
    [ 83] = K_KP_DEL,
    [ 84] = K_PRINTSCREEN,
    [ 86] = K_102ND,
    [ 87] = K_F11,
    [ 88] = K_F12,
    [ 89] = K_HOME,
    [ 90] = K_UPARROW,
    [ 91] = K_PGUP,
    [ 92] = K_LEFTARROW,
    [ 94] = K_RIGHTARROW,
    [ 95] = K_END,
    [ 96] = K_DOWNARROW,
    [ 97] = K_PGDN,
    [ 98] = K_INS,
    [ 99] = K_DEL,
    [100] = K_KP_ENTER,
    [101] = K_RCTRL,
    [102] = K_PAUSE,
    [104] = K_KP_SLASH,
    [105] = K_RALT,
    [107] = K_LWINKEY,
    [108] = K_RWINKEY,
    [109] = K_MENU,
};

const keytable_t keytable_at = {
    .keys = keys,
    .count = q_countof(keys)
};
