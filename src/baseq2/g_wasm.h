/*
Copyright (C) 1997-2001 Id Software, Inc.

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

// Game-local WASM stuff.
#define WASM_EXPORT(name) \
	__attribute__((export_name(#name)))

#define WASM_IMPORT(name) \
	__attribute__((import_module("q2"), import_name(#name)))

#define DECLARE_IMPORT(r, n, ...) \
	r wasm_ ## n(__VA_ARGS__) __WASM_IMPORT__(n)