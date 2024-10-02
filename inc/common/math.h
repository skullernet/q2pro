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

#define NUMVERTEXNORMALS    162

void vectoangles2(const vec3_t value1, vec3_t angles);

void MakeNormalVectors(const vec3_t forward, vec3_t right, vec3_t up);

extern const vec3_t bytedirs[NUMVERTEXNORMALS];

int DirToByte(const vec3_t dir);
//void ByteToDir(int index, vec3_t dir);

void SetPlaneType(cplane_t *plane);
void SetPlaneSignbits(cplane_t *plane);

typedef enum {
    BOX_INFRONT     = 1,
    BOX_BEHIND      = 2,
    BOX_INTERSECTS  = 3
} box_plane_t;

box_plane_t BoxOnPlaneSide(const vec3_t emins, const vec3_t emaxs, const cplane_t *p);

static inline box_plane_t BoxOnPlaneSideFast(const vec3_t emins, const vec3_t emaxs, const cplane_t *p)
{
    // fast axial cases
    if (p->type < 3) {
        if (p->dist <= emins[p->type])
            return BOX_INFRONT;
        if (p->dist >= emaxs[p->type])
            return BOX_BEHIND;
        return BOX_INTERSECTS;
    }

    // slow generic case
    return BoxOnPlaneSide(emins, emaxs, p);
}

static inline vec_t PlaneDiffFast(const vec3_t v, const cplane_t *p)
{
    // fast axial cases
    if (p->type < 3) {
        return v[p->type] - p->dist;
    }

    // slow generic case
    return PlaneDiff(v, p);
}

#if USE_REF

void SetupRotationMatrix(vec3_t matrix[3], const vec3_t dir, float degrees);
void RotatePointAroundVector(vec3_t out, const vec3_t dir, const vec3_t in, float degrees);

// quaternion routines, for MD5 skeletons
#if USE_MD5
typedef vec4_t quat_t;
void Quat_ComputeW(quat_t q);
void Quat_SLerp(const quat_t qa, const quat_t qb, float backlerp, float frontlerp, quat_t out);
float Quat_Normalize(quat_t q);
void Quat_MultiplyQuat(const float *restrict qa, const float *restrict qb, quat_t out);
void Quat_MultiplyVector(const float *restrict q, const float *restrict v, quat_t out);
void Quat_Conjugate(const quat_t in, quat_t out);
void Quat_RotatePoint(const quat_t q, const vec3_t in, vec3_t out);
void Quat_ToAxis(const quat_t q, vec3_t axis[3]);
#endif

#endif  // USE_REF
