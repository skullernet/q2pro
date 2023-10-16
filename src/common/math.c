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
#include "common/math.h"

#if USE_CLIENT

/*
======
vectoangles2 - this is duplicated in the game DLL, but I need it here.
======
*/
void vectoangles2(const vec3_t value1, vec3_t angles)
{
    float   forward;
    float   yaw, pitch;

    if (value1[1] == 0 && value1[0] == 0) {
        yaw = 0;
        if (value1[2] > 0)
            pitch = 90;
        else
            pitch = 270;
    } else {
        if (value1[0])
            yaw = RAD2DEG(atan2(value1[1], value1[0]));
        else if (value1[1] > 0)
            yaw = 90;
        else
            yaw = 270;

        if (yaw < 0)
            yaw += 360;

        forward = sqrtf(value1[0] * value1[0] + value1[1] * value1[1]);
        pitch = RAD2DEG(atan2(value1[2], forward));
        if (pitch < 0)
            pitch += 360;
    }

    angles[PITCH] = -pitch;
    angles[YAW] = yaw;
    angles[ROLL] = 0;
}

void MakeNormalVectors(const vec3_t forward, vec3_t right, vec3_t up)
{
    float       d;

    // this rotate and negate guarantees a vector
    // not colinear with the original
    right[1] = -forward[0];
    right[2] = forward[1];
    right[0] = forward[2];

    d = DotProduct(right, forward);
    VectorMA(right, -d, forward, right);
    VectorNormalize(right);
    CrossProduct(right, forward, up);
}

void Matrix_TransformVec4(const vec4_t a, const mat4_t m, vec4_t out)
{
    const float x = a[0];
    const float y = a[1];
    const float z = a[2];
    const float w = a[3];
    out[0] = m[0] * x + m[4] * y + m[8] * z + m[12] * w;
    out[1] = m[1] * x + m[5] * y + m[9] * z + m[13] * w;
    out[2] = m[2] * x + m[6] * y + m[10] * z + m[14] * w;
    out[3] = m[3] * x + m[7] * y + m[11] * z + m[15] * w;
}

void Matrix_Multiply(const mat4_t a, const mat4_t b, mat4_t out)
{
    const float a00 = a[0];
    const float a01 = a[1];
    const float a02 = a[2];
    const float a03 = a[3];
    const float a10 = a[4];
    const float a11 = a[5];
    const float a12 = a[6];
    const float a13 = a[7];
    const float a20 = a[8];
    const float a21 = a[9];
    const float a22 = a[10];
    const float a23 = a[11];
    const float a30 = a[12];
    const float a31 = a[13];
    const float a32 = a[14];
    const float a33 = a[15];

    // Cache only the current line of the second matrix
    float b0 = b[0];
    float b1 = b[1];
    float b2 = b[2];
    float b3 = b[3];
    out[0] = b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30;
    out[1] = b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31;
    out[2] = b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32;
    out[3] = b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33;

    b0 = b[4];
    b1 = b[5];
    b2 = b[6];
    b3 = b[7];
    out[4] = b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30;
    out[5] = b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31;
    out[6] = b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32;
    out[7] = b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33;

    b0 = b[8];
    b1 = b[9];
    b2 = b[10];
    b3 = b[11];
    out[8] = b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30;
    out[9] = b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31;
    out[10] = b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32;
    out[11] = b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33;

    b0 = b[12];
    b1 = b[13];
    b2 = b[14];
    b3 = b[15];
    out[12] = b0 * a00 + b1 * a10 + b2 * a20 + b3 * a30;
    out[13] = b0 * a01 + b1 * a11 + b2 * a21 + b3 * a31;
    out[14] = b0 * a02 + b1 * a12 + b2 * a22 + b3 * a32;
    out[15] = b0 * a03 + b1 * a13 + b2 * a23 + b3 * a33;
}

void Matrix_Frustum(float fov_x, float fov_y, float reflect_x, float znear, float zfar, float *matrix)
{
    float xmin, xmax, ymin, ymax;
    float width, height, depth;

    xmax = znear * tan(fov_x * (M_PI / 360));
    xmin = -xmax;

    ymax = znear * tan(fov_y * (M_PI / 360));
    ymin = -ymax;

    width = xmax - xmin;
    height = ymax - ymin;
    depth = zfar - znear;

    matrix[0] = reflect_x * 2 * znear / width;
    matrix[4] = 0;
    matrix[8] = (xmax + xmin) / width;
    matrix[12] = 0;

    matrix[1] = 0;
    matrix[5] = 2 * znear / height;
    matrix[9] = (ymax + ymin) / height;
    matrix[13] = 0;

    matrix[2] = 0;
    matrix[6] = 0;
    matrix[10] = -(zfar + znear) / depth;
    matrix[14] = -2 * zfar * znear / depth;

    matrix[3] = 0;
    matrix[7] = 0;
    matrix[11] = -1;
    matrix[15] = 0;
}

void Matrix_FromOriginAxis(const vec3_t origin, const vec3_t axis[3], mat4_t out)
{
    out[0] = -axis[1][0];
    out[4] = -axis[1][1];
    out[8] = -axis[1][2];
    out[12] = DotProduct(axis[1], origin);

    out[1] = axis[2][0];
    out[5] = axis[2][1];
    out[9] = axis[2][2];
    out[13] = -DotProduct(axis[2], origin);

    out[2] = -axis[0][0];
    out[6] = -axis[0][1];
    out[10] = -axis[0][2];
    out[14] = DotProduct(axis[0], origin);

    out[3] = 0;
    out[7] = 0;
    out[11] = 0;
    out[15] = 1;
}

#endif  // USE_CLIENT

const vec3_t bytedirs[NUMVERTEXNORMALS] = {
    {-0.525731, 0.000000, 0.850651}, 
    {-0.442863, 0.238856, 0.864188}, 
    {-0.295242, 0.000000, 0.955423}, 
    {-0.309017, 0.500000, 0.809017}, 
    {-0.162460, 0.262866, 0.951056}, 
    {0.000000, 0.000000, 1.000000}, 
    {0.000000, 0.850651, 0.525731}, 
    {-0.147621, 0.716567, 0.681718}, 
    {0.147621, 0.716567, 0.681718}, 
    {0.000000, 0.525731, 0.850651}, 
    {0.309017, 0.500000, 0.809017}, 
    {0.525731, 0.000000, 0.850651}, 
    {0.295242, 0.000000, 0.955423}, 
    {0.442863, 0.238856, 0.864188}, 
    {0.162460, 0.262866, 0.951056}, 
    {-0.681718, 0.147621, 0.716567}, 
    {-0.809017, 0.309017, 0.500000}, 
    {-0.587785, 0.425325, 0.688191}, 
    {-0.850651, 0.525731, 0.000000}, 
    {-0.864188, 0.442863, 0.238856}, 
    {-0.716567, 0.681718, 0.147621}, 
    {-0.688191, 0.587785, 0.425325}, 
    {-0.500000, 0.809017, 0.309017}, 
    {-0.238856, 0.864188, 0.442863}, 
    {-0.425325, 0.688191, 0.587785}, 
    {-0.716567, 0.681718, -0.147621}, 
    {-0.500000, 0.809017, -0.309017}, 
    {-0.525731, 0.850651, 0.000000}, 
    {0.000000, 0.850651, -0.525731}, 
    {-0.238856, 0.864188, -0.442863}, 
    {0.000000, 0.955423, -0.295242}, 
    {-0.262866, 0.951056, -0.162460}, 
    {0.000000, 1.000000, 0.000000}, 
    {0.000000, 0.955423, 0.295242}, 
    {-0.262866, 0.951056, 0.162460}, 
    {0.238856, 0.864188, 0.442863}, 
    {0.262866, 0.951056, 0.162460}, 
    {0.500000, 0.809017, 0.309017}, 
    {0.238856, 0.864188, -0.442863}, 
    {0.262866, 0.951056, -0.162460}, 
    {0.500000, 0.809017, -0.309017}, 
    {0.850651, 0.525731, 0.000000}, 
    {0.716567, 0.681718, 0.147621}, 
    {0.716567, 0.681718, -0.147621}, 
    {0.525731, 0.850651, 0.000000}, 
    {0.425325, 0.688191, 0.587785}, 
    {0.864188, 0.442863, 0.238856}, 
    {0.688191, 0.587785, 0.425325}, 
    {0.809017, 0.309017, 0.500000}, 
    {0.681718, 0.147621, 0.716567}, 
    {0.587785, 0.425325, 0.688191}, 
    {0.955423, 0.295242, 0.000000}, 
    {1.000000, 0.000000, 0.000000}, 
    {0.951056, 0.162460, 0.262866}, 
    {0.850651, -0.525731, 0.000000}, 
    {0.955423, -0.295242, 0.000000}, 
    {0.864188, -0.442863, 0.238856}, 
    {0.951056, -0.162460, 0.262866}, 
    {0.809017, -0.309017, 0.500000}, 
    {0.681718, -0.147621, 0.716567}, 
    {0.850651, 0.000000, 0.525731}, 
    {0.864188, 0.442863, -0.238856}, 
    {0.809017, 0.309017, -0.500000}, 
    {0.951056, 0.162460, -0.262866}, 
    {0.525731, 0.000000, -0.850651}, 
    {0.681718, 0.147621, -0.716567}, 
    {0.681718, -0.147621, -0.716567}, 
    {0.850651, 0.000000, -0.525731}, 
    {0.809017, -0.309017, -0.500000}, 
    {0.864188, -0.442863, -0.238856}, 
    {0.951056, -0.162460, -0.262866}, 
    {0.147621, 0.716567, -0.681718}, 
    {0.309017, 0.500000, -0.809017}, 
    {0.425325, 0.688191, -0.587785}, 
    {0.442863, 0.238856, -0.864188}, 
    {0.587785, 0.425325, -0.688191}, 
    {0.688191, 0.587785, -0.425325}, 
    {-0.147621, 0.716567, -0.681718}, 
    {-0.309017, 0.500000, -0.809017}, 
    {0.000000, 0.525731, -0.850651}, 
    {-0.525731, 0.000000, -0.850651}, 
    {-0.442863, 0.238856, -0.864188}, 
    {-0.295242, 0.000000, -0.955423}, 
    {-0.162460, 0.262866, -0.951056}, 
    {0.000000, 0.000000, -1.000000}, 
    {0.295242, 0.000000, -0.955423}, 
    {0.162460, 0.262866, -0.951056}, 
    {-0.442863, -0.238856, -0.864188}, 
    {-0.309017, -0.500000, -0.809017}, 
    {-0.162460, -0.262866, -0.951056}, 
    {0.000000, -0.850651, -0.525731}, 
    {-0.147621, -0.716567, -0.681718}, 
    {0.147621, -0.716567, -0.681718}, 
    {0.000000, -0.525731, -0.850651}, 
    {0.309017, -0.500000, -0.809017}, 
    {0.442863, -0.238856, -0.864188}, 
    {0.162460, -0.262866, -0.951056}, 
    {0.238856, -0.864188, -0.442863}, 
    {0.500000, -0.809017, -0.309017}, 
    {0.425325, -0.688191, -0.587785}, 
    {0.716567, -0.681718, -0.147621}, 
    {0.688191, -0.587785, -0.425325}, 
    {0.587785, -0.425325, -0.688191}, 
    {0.000000, -0.955423, -0.295242}, 
    {0.000000, -1.000000, 0.000000}, 
    {0.262866, -0.951056, -0.162460}, 
    {0.000000, -0.850651, 0.525731}, 
    {0.000000, -0.955423, 0.295242}, 
    {0.238856, -0.864188, 0.442863}, 
    {0.262866, -0.951056, 0.162460}, 
    {0.500000, -0.809017, 0.309017}, 
    {0.716567, -0.681718, 0.147621}, 
    {0.525731, -0.850651, 0.000000}, 
    {-0.238856, -0.864188, -0.442863}, 
    {-0.500000, -0.809017, -0.309017}, 
    {-0.262866, -0.951056, -0.162460}, 
    {-0.850651, -0.525731, 0.000000}, 
    {-0.716567, -0.681718, -0.147621}, 
    {-0.716567, -0.681718, 0.147621}, 
    {-0.525731, -0.850651, 0.000000}, 
    {-0.500000, -0.809017, 0.309017}, 
    {-0.238856, -0.864188, 0.442863}, 
    {-0.262866, -0.951056, 0.162460}, 
    {-0.864188, -0.442863, 0.238856}, 
    {-0.809017, -0.309017, 0.500000}, 
    {-0.688191, -0.587785, 0.425325}, 
    {-0.681718, -0.147621, 0.716567}, 
    {-0.442863, -0.238856, 0.864188}, 
    {-0.587785, -0.425325, 0.688191}, 
    {-0.309017, -0.500000, 0.809017}, 
    {-0.147621, -0.716567, 0.681718}, 
    {-0.425325, -0.688191, 0.587785}, 
    {-0.162460, -0.262866, 0.951056}, 
    {0.442863, -0.238856, 0.864188}, 
    {0.162460, -0.262866, 0.951056}, 
    {0.309017, -0.500000, 0.809017}, 
    {0.147621, -0.716567, 0.681718}, 
    {0.000000, -0.525731, 0.850651}, 
    {0.425325, -0.688191, 0.587785}, 
    {0.587785, -0.425325, 0.688191}, 
    {0.688191, -0.587785, 0.425325}, 
    {-0.955423, 0.295242, 0.000000}, 
    {-0.951056, 0.162460, 0.262866}, 
    {-1.000000, 0.000000, 0.000000}, 
    {-0.850651, 0.000000, 0.525731}, 
    {-0.955423, -0.295242, 0.000000}, 
    {-0.951056, -0.162460, 0.262866}, 
    {-0.864188, 0.442863, -0.238856}, 
    {-0.951056, 0.162460, -0.262866}, 
    {-0.809017, 0.309017, -0.500000}, 
    {-0.864188, -0.442863, -0.238856}, 
    {-0.951056, -0.162460, -0.262866}, 
    {-0.809017, -0.309017, -0.500000}, 
    {-0.681718, 0.147621, -0.716567}, 
    {-0.681718, -0.147621, -0.716567}, 
    {-0.850651, 0.000000, -0.525731}, 
    {-0.688191, 0.587785, -0.425325}, 
    {-0.587785, 0.425325, -0.688191}, 
    {-0.425325, 0.688191, -0.587785}, 
    {-0.425325, -0.688191, -0.587785}, 
    {-0.587785, -0.425325, -0.688191}, 
    {-0.688191, -0.587785, -0.425325}, 
};

int DirToByte(const vec3_t dir)
{
    int     i, best;
    float   d, bestd;

    if (!dir) {
        return 0;
    }

    bestd = 0;
    best = 0;
    for (i = 0; i < NUMVERTEXNORMALS; i++) {
        d = DotProduct(dir, bytedirs[i]);
        if (d > bestd) {
            bestd = d;
            best = i;
        }
    }

    return best;
}

#if 0
void ByteToDir(int index, vec3_t dir)
{
    if (index < 0 || index >= NUMVERTEXNORMALS) {
        Com_Error(ERR_FATAL, "ByteToDir: illegal index");
    }

    VectorCopy(bytedirs[index], dir);
}
#endif

void SetPlaneType(cplane_t *plane)
{
    vec_t *normal = plane->normal;

    if (normal[0] == 1) {
        plane->type = PLANE_X;
        return;
    }
    if (normal[1] == 1) {
        plane->type = PLANE_Y;
        return;
    }
    if (normal[2] == 1) {
        plane->type = PLANE_Z;
        return;
    }

    plane->type = PLANE_NON_AXIAL;
}

void SetPlaneSignbits(cplane_t *plane)
{
    int bits = 0;

    if (plane->normal[0] < 0) {
        bits |= 1;
    }
    if (plane->normal[1] < 0) {
        bits |= 2;
    }
    if (plane->normal[2] < 0) {
        bits |= 4;
    }

    plane->signbits = bits;
}

/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2
==================
*/
int BoxOnPlaneSide(const vec3_t emins, const vec3_t emaxs, const cplane_t *p)
{
    const vec_t *bounds[2] = { emins, emaxs };
    int     i = p->signbits & 1;
    int     j = (p->signbits >> 1) & 1;
    int     k = (p->signbits >> 2) & 1;

#define P(i, j, k) \
    p->normal[0] * bounds[i][0] + \
    p->normal[1] * bounds[j][1] + \
    p->normal[2] * bounds[k][2]

    vec_t   dist1 = P(i ^ 1, j ^ 1, k ^ 1);
    vec_t   dist2 = P(i, j, k);
    int     sides = 0;

#undef P

    if (dist1 >= p->dist)
        sides = BOX_INFRONT;
    if (dist2 < p->dist)
        sides |= BOX_BEHIND;

    return sides;
}

#if USE_REF

/*
==================
SetupRotationMatrix

Setup rotation matrix given the normalized direction vector and angle to rotate
around this vector. Adapted from Mesa 3D implementation of _math_matrix_rotate.
==================
*/
void SetupRotationMatrix(vec3_t matrix[3], const vec3_t dir, float degrees)
{
    vec_t   angle, s, c, one_c, xx, yy, zz, xy, yz, zx, xs, ys, zs;

    angle = DEG2RAD(degrees);
    s = sin(angle);
    c = cos(angle);
    one_c = 1.0F - c;

    xx = dir[0] * dir[0];
    yy = dir[1] * dir[1];
    zz = dir[2] * dir[2];
    xy = dir[0] * dir[1];
    yz = dir[1] * dir[2];
    zx = dir[2] * dir[0];
    xs = dir[0] * s;
    ys = dir[1] * s;
    zs = dir[2] * s;

    matrix[0][0] = (one_c * xx) + c;
    matrix[0][1] = (one_c * xy) - zs;
    matrix[0][2] = (one_c * zx) + ys;

    matrix[1][0] = (one_c * xy) + zs;
    matrix[1][1] = (one_c * yy) + c;
    matrix[1][2] = (one_c * yz) - xs;

    matrix[2][0] = (one_c * zx) - ys;
    matrix[2][1] = (one_c * yz) + xs;
    matrix[2][2] = (one_c * zz) + c;
}

#if USE_MD5

#define X 0
#define Y 1
#define Z 2
#define W 3

void Quat_ComputeW(quat_t q)
{
    float t = 1.0f - (q[X] * q[X]) - (q[Y] * q[Y]) - (q[Z] * q[Z]);

    if (t < 0.0f) {
        q[W] = 0.0f;
    } else {
        q[W] = -sqrtf(t);
    }
}

#define QUAT_EPSILON 0.000001f

void Quat_SLerp(const quat_t qa, const quat_t qb, float backlerp, float frontlerp, quat_t out)
{
    if (backlerp <= 0.0f) {
        Vector4Copy(qb, out);
        return;
    } else if (backlerp >= 1.0f) {
        Vector4Copy(qa, out);
        return;
    }

    // compute "cosine of angle between quaternions" using dot product
    float cosOmega = Dot4Product(qa, qb);

    /* If negative dot, use -q1.  Two quaternions q and -q
       represent the same rotation, but may produce
       different slerp.  We chose q or -q to rotate using
       the acute angle. */
    float q1w = qb[W];
    float q1x = qb[X];
    float q1y = qb[Y];
    float q1z = qb[Z];

    if (cosOmega < 0.0f) {
        q1w = -q1w;
        q1x = -q1x;
        q1y = -q1y;
        q1z = -q1z;
        cosOmega = -cosOmega;
    }

    // compute interpolation fraction
    float k0, k1;

    if (1.0f - cosOmega <= QUAT_EPSILON) {
        // very close - just use linear interpolation
        k0 = backlerp;
        k1 = frontlerp;
    } else {
        // compute the sin of the angle using the trig identity sin^2(omega) + cos^2(omega) = 1
        float sinOmega = sqrtf(1.0f - (cosOmega * cosOmega));

        // compute the angle from its sin and cosine
        float omega = atan2f(sinOmega, cosOmega);
        float oneOverSinOmega = 1.0f / sinOmega;

        k0 = sinf(backlerp * omega) * oneOverSinOmega;
        k1 = sinf(frontlerp * omega) * oneOverSinOmega;
    }

    out[W] = (k0 * qa[W]) + (k1 * q1w);
    out[X] = (k0 * qa[X]) + (k1 * q1x);
    out[Y] = (k0 * qa[Y]) + (k1 * q1y);
    out[Z] = (k0 * qa[Z]) + (k1 * q1z);
}

float Quat_Normalize(quat_t q)
{
    float length = sqrtf(Dot4Product(q, q));

    if (length) {
        float ilength = 1 / length;
        q[X] *= ilength;
        q[Y] *= ilength;
        q[Z] *= ilength;
        q[W] *= ilength;
    }

    return length;
}

#endif  // USE_MD5

#endif  // USE_REF
