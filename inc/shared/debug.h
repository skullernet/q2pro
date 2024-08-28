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

#define DEBUG_DRAW_API_V1 "DEBUG_DRAW_API_V1"

typedef struct {
    void (*ClearDebugLines)(void);
    void (*AddDebugLine)(const vec3_t start, const vec3_t end, uint32_t color, uint32_t time, qboolean depth_test);
    void (*AddDebugPoint)(const vec3_t point, float size, uint32_t color, uint32_t time, qboolean depth_test);
    void (*AddDebugAxis)(const vec3_t origin, const vec3_t angles, float size, uint32_t time, qboolean depth_test);
    void (*AddDebugBounds)(const vec3_t mins, const vec3_t maxs, uint32_t color, uint32_t time, qboolean depth_test);
    void (*AddDebugSphere)(const vec3_t origin, float radius, uint32_t color, uint32_t time, qboolean depth_test);
    void (*AddDebugCircle)(const vec3_t origin, float radius, uint32_t color, uint32_t time, qboolean depth_test);
    void (*AddDebugCylinder)(const vec3_t origin, float half_height, float radius, uint32_t color, uint32_t time, qboolean depth_test);
    void (*AddDebugArrow)(const vec3_t start, const vec3_t end, float size, uint32_t line_color,
                          uint32_t arrow_color, uint32_t time, qboolean depth_test);
    void (*AddDebugCurveArrow)(const vec3_t start, const vec3_t ctrl, const vec3_t end, float size,
                               uint32_t line_color, uint32_t arrow_color, uint32_t time, qboolean depth_test);
    void (*AddDebugText)(const vec3_t origin, const vec3_t angles, const char *text,
                         float size, uint32_t color, uint32_t time, qboolean depth_test);
} debug_draw_api_v1_t;
