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

#define BOX_INFRONT     1
#define BOX_BEHIND      2
#define BOX_INTERSECTS  3

int BoxOnPlaneSide(const vec3_t emins, const vec3_t emaxs, const cplane_t *p);

static inline int BoxOnPlaneSideFast(const vec3_t emins, const vec3_t emaxs, const cplane_t *p)
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

void Matrix_TransformVec4(const vec4_t a, const mat4_t m, vec4_t out);
void Matrix_Multiply(const mat4_t a, const mat4_t b, mat4_t out);
void Matrix_Frustum(float fov_x, float fov_y, float reflect_x, float znear, float zfar, float *matrix);
void Matrix_FromOriginAxis(const vec3_t origin, const vec3_t axis[3], mat4_t out);

// quaternion routines, for MD5 skeletons
#if USE_MD5

#define X 0
#define Y 1
#define Z 2
#define W 3

typedef vec4_t quat_t;

void Quat_ComputeW(quat_t q);
void Quat_SLerp(const quat_t qa, const quat_t qb, float backlerp, float frontlerp, quat_t out);
float Quat_Normalize(quat_t q);

static inline void Quat_MultiplyQuat(const quat_t qa, const quat_t qb, quat_t out)
{
    out[W] = (qa[W] * qb[W]) - (qa[X] * qb[X]) - (qa[Y] * qb[Y]) - (qa[Z] * qb[Z]);
    out[X] = (qa[X] * qb[W]) + (qa[W] * qb[X]) + (qa[Y] * qb[Z]) - (qa[Z] * qb[Y]);
    out[Y] = (qa[Y] * qb[W]) + (qa[W] * qb[Y]) + (qa[Z] * qb[X]) - (qa[X] * qb[Z]);
    out[Z] = (qa[Z] * qb[W]) + (qa[W] * qb[Z]) + (qa[X] * qb[Y]) - (qa[Y] * qb[X]);
}

static inline void Quat_MultiplyVector(const quat_t q, const vec3_t v, quat_t out)
{
    out[W] = -(q[X] * v[X]) - (q[Y] * v[Y]) - (q[Z] * v[Z]);
    out[X] = (q[W] * v[X]) + (q[Y] * v[Z]) - (q[Z] * v[Y]);
    out[Y] = (q[W] * v[Y]) + (q[Z] * v[X]) - (q[X] * v[Z]);
    out[Z] = (q[W] * v[Z]) + (q[X] * v[Y]) - (q[Y] * v[X]);
}

// Conjugate quaternion. Also, inverse, for unit quaternions (which MD5 quats are)
static inline void Quat_Conjugate(const quat_t in, quat_t out)
{
    out[W] = in[W];
    out[X] = -in[X];
    out[Y] = -in[Y];
    out[Z] = -in[Z];
}

static inline void Quat_RotatePoint(const quat_t q, const vec3_t in, vec3_t out)
{
    quat_t tmp, inv, output;

    // Assume q is unit quaternion
    Quat_Conjugate(q, inv);
    Quat_MultiplyVector(q, in, tmp);
    Quat_MultiplyQuat(tmp, inv, output);

    out[X] = output[X];
    out[Y] = output[Y];
    out[Z] = output[Z];
}

#undef X
#undef Y
#undef Z
#undef W

#endif

#endif  // USE_REF
