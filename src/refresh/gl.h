/*
Copyright (C) 2003-2006 Andrey Nazarov

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

#include "shared/shared.h"
#include "common/bsp.h"
#include "common/cmd.h"
#include "common/common.h"
#include "common/cvar.h"
#include "common/files.h"
#include "common/hash_map.h"
#include "common/intreadwrite.h"
#include "common/math.h"
#include "client/video.h"
#include "client/client.h"
#include "refresh/refresh.h"
#include "system/hunk.h"
#include "images.h"
#include "qgl.h"

/*
 * gl_main.c
 *
 */

#if USE_GLES
#define QGL_INDEX_TYPE  GLushort
#define QGL_INDEX_ENUM  GL_UNSIGNED_SHORT
#else
#define QGL_INDEX_TYPE  GLuint
#define QGL_INDEX_ENUM  GL_UNSIGNED_INT
#endif

#define MAX_TMUS        3

#define TAB_SIN(x) gl_static.sintab[(x) & 255]
#define TAB_COS(x) gl_static.sintab[((x) + 64) & 255]

#define MAX_PROGRAMS    128
#define NUM_TEXNUMS     7

typedef struct {
    uint8_t size;
    bool type;
    uint8_t stride;
    uint8_t offset;
} glVaDesc_t;

typedef struct {
    const char *name;

    void (*init)(void);
    void (*shutdown)(void);
    void (*clear_state)(void);
    void (*setup_2d)(void);
    void (*setup_3d)(void);

    void (*load_proj_matrix)(const GLfloat *matrix);
    void (*load_view_matrix)(const GLfloat *matrix);

    void (*state_bits)(GLbitfield bits);
    void (*array_bits)(GLbitfield bits);

    void (*array_pointers)(const glVaDesc_t *desc, const GLfloat *ptr);
    void (*tex_coord_pointer)(const GLfloat *ptr);

    void (*color)(GLfloat r, GLfloat g, GLfloat b, GLfloat a);
} glbackend_t;

typedef struct {
    GLuint query;
    float frac;
    unsigned timestamp;
    bool pending;
    bool visible;
} glquery_t;

typedef enum {
    VA_NONE,

    VA_SPRITE,
    VA_EFFECT,
    VA_NULLMODEL,
    VA_OCCLUDE,
    VA_WATERWARP,
    VA_MESH_SHADE,
    VA_MESH_FLAT,
    VA_2D,
    VA_3D,

    VA_TOTAL
} glVertexArray_t;

typedef struct {
    bool            registering;
    bool            use_shaders;
    struct {
        bsp_t       *cache;
        vec_t       *vertices;
        GLuint      bufnum;
        vec_t       size;
    } world;
    GLuint          warp_texture;
    GLuint          warp_renderbuffer;
    GLuint          warp_framebuffer;
    GLuint          u_bufnum;
    GLuint          array_object;
    GLuint          index_buffer;
    GLuint          vertex_buffer;
    GLuint          programs[MAX_PROGRAMS];
    GLuint          texnums[NUM_TEXNUMS];
    GLenum          samples_passed;
    GLbitfield      stencil_buffer_bit;
    float           entity_modulate;
    uint32_t        inverse_intensity_33;
    uint32_t        inverse_intensity_66;
    uint32_t        inverse_intensity_100;
    int             nolm_mask;
    float           sintab[256];
    byte            latlngtab[NUMVERTEXNORMALS][2];
    byte            lightstylemap[MAX_LIGHTSTYLES];
    hash_map_t      *queries;
} glStatic_t;

typedef struct {
    refdef_t        fd;
    vec3_t          viewaxis[3];
    GLfloat         viewmatrix[16];
    unsigned        visframe;
    unsigned        drawframe;
    unsigned        dlightframe;
    unsigned        rand_seed;
    unsigned        timestamp;
    float           frametime;
    int             viewcluster1;
    int             viewcluster2;
    int             nodes_visible;
    cplane_t        frustumPlanes[4];
    entity_t        *ent;
    bool            entrotated;
    float           entscale;
    vec3_t          entaxis[3];
    GLfloat         entmatrix[16];
    lightpoint_t    lightpoint;
    int             num_beams;
    int             num_flares;
    int             framebuffer_width;
    int             framebuffer_height;
    bool            framebuffer_ok;
} glRefdef_t;

typedef enum {
    QGL_CAP_LEGACY                      = BIT(0),
    QGL_CAP_SHADER                      = BIT(1),
    QGL_CAP_TEXTURE_BITS                = BIT(2),
    QGL_CAP_TEXTURE_CLAMP_TO_EDGE       = BIT(3),
    QGL_CAP_TEXTURE_MAX_LEVEL           = BIT(4),
    QGL_CAP_TEXTURE_LOD_BIAS            = BIT(5),
    QGL_CAP_TEXTURE_NON_POWER_OF_TWO    = BIT(6),
    QGL_CAP_TEXTURE_ANISOTROPY          = BIT(7),
    QGL_CAP_UNPACK_SUBIMAGE             = BIT(8),
    QGL_CAP_QUERY_RESULT_NO_WAIT        = BIT(9),
    QGL_CAP_CLIENT_VA                   = BIT(10),
} glcap_t;

#define QGL_VER(major, minor)   ((major) * 100 + (minor))
#define QGL_UNPACK_VER(ver)     (ver) / 100, (ver) % 100

typedef struct {
    int     ver_gl;
    int     ver_es;
    int     ver_sl;
    glcap_t caps;
    int     colorbits;
    int     depthbits;
    int     stencilbits;
    int     max_texture_size_log2;
    int     max_texture_size;
} glConfig_t;

extern glStatic_t gl_static;
extern glConfig_t gl_config;
extern glRefdef_t glr;

extern entity_t gl_world;

extern unsigned r_registration_sequence;

typedef struct {
    int nodesDrawn;
    int leavesDrawn;
    int facesMarked;
    int facesDrawn;
    int facesTris;
    int texSwitches;
    int texUploads;
    int lightTexels;
    int trisDrawn;
    int batchesDrawn;
    int nodesCulled;
    int facesCulled;
    int boxesCulled;
    int spheresCulled;
    int rotatedBoxesCulled;
    int batchesDrawn2D;
    int uniformUploads;
    int vertexArrayBinds;
    int occlusionQueries;
} statCounters_t;

extern statCounters_t c;

// regular variables
extern cvar_t *gl_partscale;
extern cvar_t *gl_partstyle;
extern cvar_t *gl_celshading;
extern cvar_t *gl_dotshading;
extern cvar_t *gl_shadows;
extern cvar_t *gl_modulate;
extern cvar_t *gl_modulate_world;
extern cvar_t *gl_coloredlightmaps;
extern cvar_t *gl_lightmap_bits;
extern cvar_t *gl_brightness;
extern cvar_t *gl_dynamic;
extern cvar_t *gl_dlight_falloff;
extern cvar_t *gl_modulate_entities;
extern cvar_t *gl_doublelight_entities;
extern cvar_t *gl_glowmap_intensity;
extern cvar_t *gl_flarespeed;
extern cvar_t *gl_fontshadow;
extern cvar_t *gl_shaders;
#if USE_MD5
extern cvar_t *gl_md5_load;
extern cvar_t *gl_md5_use;
extern cvar_t *gl_md5_distance;
#endif
extern cvar_t *gl_damageblend_frac;

// development variables
extern cvar_t *gl_znear;
extern cvar_t *gl_drawsky;
extern cvar_t *gl_showtris;
#if USE_DEBUG
extern cvar_t *gl_nobind;
extern cvar_t *gl_test;
#endif
extern cvar_t *gl_cull_nodes;
extern cvar_t *gl_clear;
extern cvar_t *gl_novis;
extern cvar_t *gl_lockpvs;
extern cvar_t *gl_lightmap;
extern cvar_t *gl_fullbright;
extern cvar_t *gl_vertexlight;
extern cvar_t *gl_lightgrid;
extern cvar_t *gl_showerrors;

#define GL_rand()   Q_rand_state(&glr.rand_seed)
#define GL_frand()  ((int32_t)GL_rand() * 0x1p-32f + 0.5f)

typedef enum {
    CULL_OUT,
    CULL_IN,
    CULL_CLIP
} glCullResult_t;

glCullResult_t GL_CullBox(const vec3_t bounds[2]);
glCullResult_t GL_CullSphere(const vec3_t origin, float radius);
glCullResult_t GL_CullLocalBox(const vec3_t origin, const vec3_t bounds[2]);

bool GL_AllocBlock(int width, int height, uint16_t *inuse,
                   int w, int h, int *s, int *t);

void GL_MultMatrix(GLfloat *restrict out, const GLfloat *restrict a, const GLfloat *restrict b);
void GL_SetEntityAxis(void);
void GL_RotationMatrix(GLfloat *matrix);
void GL_RotateForEntity(void);

void GL_ClearErrors(void);
bool GL_ShowErrors(const char *func);

void GL_InitQueries(void);
void GL_DeleteQueries(void);

static inline void GL_AdvanceValue(float *restrict val, float target, float speed)
{
    if (speed <= 0) {
        *val = target;
    } else if (*val < target) {
        *val += speed * glr.frametime;
        if (*val > target)
            *val = target;
    } else if (*val > target) {
        *val -= speed * glr.frametime;
        if (*val < target)
            *val = target;
    }
}

/*
 * gl_model.c
 *
 */

typedef struct {
    float   st[2];
} maliastc_t;

typedef struct {
    int16_t     pos[3];
    uint8_t     norm[2]; // lat, lng
} maliasvert_t;

typedef struct {
    vec3_t  scale;
    vec3_t  translate;
    vec3_t  bounds[2];
    vec_t   radius;
} maliasframe_t;

typedef char maliasskinname_t[MAX_QPATH];

typedef struct {
    int             numverts;
    int             numtris;
    int             numindices;
    int             numskins;
    QGL_INDEX_TYPE  *indices;
    maliasvert_t    *verts;
    maliastc_t      *tcoords;
#if USE_MD5
    maliasskinname_t *skinnames;
#endif
    image_t         **skins;
} maliasmesh_t;

typedef struct {
    int             width;
    int             height;
    int             origin_x;
    int             origin_y;
    image_t         *image;
} mspriteframe_t;

#if USE_MD5

// the total amount of joints the renderer will bother handling
#define MD5_MAX_JOINTS      256
#define MD5_MAX_JOINTNAME   32
#define MD5_MAX_MESHES      32
#define MD5_MAX_WEIGHTS     8192
#define MD5_MAX_FRAMES      1024

/* Joint */
typedef struct {
    int parent;

    vec3_t pos;
    quat_t orient;
    float scale;
} md5_joint_t;

/* Vertex */
typedef struct {
    vec3_t normal;

    uint32_t start; /* start weight */
    uint32_t count; /* weight count */
} md5_vertex_t;

/* Weight */
typedef struct {
    int joint;
    float bias;

    vec3_t pos;
} md5_weight_t;

/* Mesh */
typedef struct {
    int num_verts;
    int num_indices;
    int num_weights;

    md5_vertex_t *vertices;
    maliastc_t *tcoords;
    QGL_INDEX_TYPE *indices;
    md5_weight_t *weights;
} md5_mesh_t;

/* MD5 model + animation structure */
typedef struct {
    int num_meshes;
    int num_joints;
    int num_frames; // may not match model_t::numframes, but not fatal
    int num_skins;

    md5_mesh_t *meshes;
    md5_joint_t *base_skeleton;
    md5_joint_t *skeleton_frames; // [num_joints][num_frames]
    image_t **skins;
} md5_model_t;

#endif

typedef struct {
    enum {
        MOD_FREE,
        MOD_ALIAS,
        MOD_SPRITE,
        MOD_EMPTY
    } type;

    char name[MAX_QPATH];
    unsigned registration_sequence;
    memhunk_t hunk;

    int nummeshes;
    int numframes;

    maliasmesh_t *meshes; // md2 / md3
#if USE_MD5
    md5_model_t *skeleton; // md5
    memhunk_t skeleton_hunk; // md5
#endif
    union {
        maliasframe_t *frames;
        mspriteframe_t *spriteframes;
    };

    GLuint buffer;
} model_t;

// xyz[3] | color[1]  | st[2]    | lmst[2]
// xyz[3] | unused[1] | color[4]
#define VERTEX_SIZE 8

void MOD_FreeUnused(void);
void MOD_FreeAll(void);
void MOD_Init(void);
void MOD_Shutdown(void);

model_t *MOD_ForHandle(qhandle_t h);
qhandle_t R_RegisterModel(const char *name);

/*
 * gl_surf.c
 *
 */
#define LIGHT_STYLE(i) \
    &glr.fd.lightstyles[gl_static.lightstylemap[(i)]]

#define LM_MAX_LIGHTMAPS    128
#define LM_MAX_BLOCK_WIDTH  (1 << 10)

typedef struct lightmap_s {
    uint16_t    mins[2];
    uint16_t    maxs[2];
    byte        *buffer;
} lightmap_t;

typedef struct {
    bool        dirty;
    int         comp, block_size, block_shift;
    float       add, modulate, scale;
    int         nummaps, maxmaps, block_bytes;
    uint16_t    inuse[LM_MAX_BLOCK_WIDTH];
    GLuint      texnums[LM_MAX_LIGHTMAPS];
    lightmap_t  lightmaps[LM_MAX_LIGHTMAPS];
} lightmap_builder_t;

extern lightmap_builder_t lm;

void GL_AdjustColor(vec3_t color);
void GL_PushLights(mface_t *surf);
void GL_UploadLightmaps(void);

void GL_RebuildLighting(void);
void GL_FreeWorld(void);
void GL_LoadWorld(const char *name);

/*
 * gl_state.c
 *
 */
typedef enum {
    GLS_DEFAULT             = 0,
    GLS_DEPTHMASK_FALSE     = BIT(0),
    GLS_DEPTHTEST_DISABLE   = BIT(1),
    GLS_CULL_DISABLE        = BIT(2),
    GLS_BLEND_BLEND         = BIT(3),
    GLS_BLEND_ADD           = BIT(4),
    GLS_BLEND_MODULATE      = BIT(5),
    GLS_ALPHATEST_ENABLE    = BIT(6),
    GLS_TEXTURE_REPLACE     = BIT(7),
    GLS_SCROLL_ENABLE       = BIT(8),
    GLS_LIGHTMAP_ENABLE     = BIT(9),
    GLS_WARP_ENABLE         = BIT(10),
    GLS_INTENSITY_ENABLE    = BIT(11),
    GLS_GLOWMAP_ENABLE      = BIT(12),
    GLS_SHADE_SMOOTH        = BIT(13),
    GLS_SCROLL_X            = BIT(14),
    GLS_SCROLL_Y            = BIT(15),
    GLS_SCROLL_FLIP         = BIT(16),
    GLS_SCROLL_SLOW         = BIT(17),

    GLS_BLEND_MASK  = GLS_BLEND_BLEND | GLS_BLEND_ADD | GLS_BLEND_MODULATE,
    GLS_COMMON_MASK = GLS_DEPTHMASK_FALSE | GLS_DEPTHTEST_DISABLE | GLS_CULL_DISABLE | GLS_BLEND_MASK,
    GLS_SHADER_MASK = GLS_ALPHATEST_ENABLE | GLS_TEXTURE_REPLACE | GLS_SCROLL_ENABLE |
        GLS_LIGHTMAP_ENABLE | GLS_WARP_ENABLE | GLS_INTENSITY_ENABLE | GLS_GLOWMAP_ENABLE,
    GLS_SCROLL_MASK = GLS_SCROLL_ENABLE | GLS_SCROLL_X | GLS_SCROLL_Y | GLS_SCROLL_FLIP | GLS_SCROLL_SLOW,
} glStateBits_t;

typedef enum {
    VERT_ATTR_POS,
    VERT_ATTR_TC,
    VERT_ATTR_LMTC,
    VERT_ATTR_COLOR,

    VERT_ATTR_COUNT
} glVertexAttr_t;

typedef enum {
    GLA_NONE        = 0,
    GLA_VERTEX      = BIT(VERT_ATTR_POS),
    GLA_TC          = BIT(VERT_ATTR_TC),
    GLA_LMTC        = BIT(VERT_ATTR_LMTC),
    GLA_COLOR       = BIT(VERT_ATTR_COLOR),
} glArrayBits_t;

typedef struct {
    GLuint          client_tmu;
    GLuint          server_tmu;
    GLuint          texnums[MAX_TMUS];
    GLbitfield      state_bits;
    GLbitfield      array_bits;
    GLuint          currentbuffer[2];
    glVertexArray_t currentva;
    const GLfloat   *currentmatrix;
    struct {
        GLfloat     view[16];
        GLfloat     proj[16];
        GLfloat     time;
        GLfloat     modulate;
        GLfloat     add;
        GLfloat     intensity;
        GLfloat     intensity2;
        GLfloat     pad;
        GLfloat     w_amp[2];
        GLfloat     w_phase[2];
        GLfloat     scroll[2];
    } u_block;
} glState_t;

extern glState_t gls;

extern const glbackend_t *gl_backend;

static inline void GL_ActiveTexture(GLuint tmu)
{
    if (gls.server_tmu != tmu) {
        qglActiveTexture(GL_TEXTURE0 + tmu);
        gls.server_tmu = tmu;
    }
}

static inline void GL_ClientActiveTexture(GLuint tmu)
{
    if (gls.client_tmu != tmu) {
        qglClientActiveTexture(GL_TEXTURE0 + tmu);
        gls.client_tmu = tmu;
    }
}

static inline void GL_StateBits(GLbitfield bits)
{
    if (gls.state_bits != bits) {
        gl_backend->state_bits(bits);
        gls.state_bits = bits;
    }
}

static inline void GL_ArrayBits(GLbitfield bits)
{
    if (gls.array_bits != bits) {
        gl_backend->array_bits(bits);
        gls.array_bits = bits;
    }
}

static inline void GL_ForceMatrix(const GLfloat *matrix)
{
    gl_backend->load_view_matrix(matrix);
    gls.currentmatrix = matrix;
}

static inline void GL_LoadMatrix(const GLfloat *matrix)
{
    if (gls.currentmatrix != matrix) {
        gl_backend->load_view_matrix(matrix);
        gls.currentmatrix = matrix;
    }
}

static inline void GL_BindBuffer(GLenum target, GLuint buffer)
{
    const int i = target == GL_ELEMENT_ARRAY_BUFFER;
    if (gls.currentbuffer[i] != buffer) {
        qglBindBuffer(target, buffer);
        gls.currentbuffer[i] = buffer;
    }
}

static inline void GL_ClearDepth(GLfloat d)
{
    if (qglClearDepthf)
        qglClearDepthf(d);
    else
        qglClearDepth(d);
}

static inline void GL_DepthRange(GLfloat n, GLfloat f)
{
    if (qglDepthRangef)
        qglDepthRangef(n, f);
    else
        qglDepthRange(n, f);
}

#define GL_Color(r, g, b, a) gl_backend->color(r, g, b, a)

#define GL_DrawTriangles(num_indices, indices) \
    qglDrawElements(GL_TRIANGLES, num_indices, QGL_INDEX_ENUM, indices)

typedef enum {
    SHOWTRIS_WORLD  = BIT(0),
    SHOWTRIS_MESH   = BIT(1),
    SHOWTRIS_PIC    = BIT(2),
    SHOWTRIS_FX     = BIT(3),
} showtris_t;

void GL_ForceTexture(GLuint tmu, GLuint texnum);
void GL_BindTexture(GLuint tmu, GLuint texnum);
void GL_CommonStateBits(GLbitfield bits);
void GL_ScrollSpeed(vec2_t scroll, GLbitfield bits);
void GL_DrawOutlines(GLsizei count, const QGL_INDEX_TYPE *indices, bool indexed);
void GL_Ortho(GLfloat xmin, GLfloat xmax, GLfloat ymin, GLfloat ymax, GLfloat znear, GLfloat zfar);
void GL_Frustum(GLfloat fov_x, GLfloat fov_y, GLfloat reflect_x);
void GL_Setup2D(void);
void GL_Setup3D(bool waterwarp);
void GL_ClearState(void);
void GL_InitState(void);
void GL_ShutdownState(void);

extern const glbackend_t backend_legacy;
extern const glbackend_t backend_shader;

/*
 * gl_draw.c
 *
 */
typedef struct {
    color_t     colors[2]; // 0 - actual color, 1 - transparency (for text drawing)
    bool        scissor;
    float       scale;
} drawStatic_t;

extern drawStatic_t draw;

#if USE_DEBUG
extern qhandle_t r_charset;

void Draw_Stats(void);
void Draw_Lightmaps(void);
void Draw_Scrap(void);
#endif

void GL_Blend(void);


/*
 * gl_images.c
 *
 */

// auto textures
#define TEXNUM_DEFAULT  gl_static.texnums[0]
#define TEXNUM_SCRAP    gl_static.texnums[1]
#define TEXNUM_PARTICLE gl_static.texnums[2]
#define TEXNUM_BEAM     gl_static.texnums[3]
#define TEXNUM_WHITE    gl_static.texnums[4]
#define TEXNUM_BLACK    gl_static.texnums[5]
#define TEXNUM_RAW      gl_static.texnums[6]

void Scrap_Upload(void);

void GL_InitImages(void);
void GL_ShutdownImages(void);

bool GL_InitWarpTexture(void);

extern cvar_t *gl_intensity;


/*
 * gl_tess.c
 *
 */
#define TESS_MAX_VERTICES   6144
#define TESS_MAX_INDICES    (3 * TESS_MAX_VERTICES)

typedef struct {
    GLfloat         vertices[VERTEX_SIZE * TESS_MAX_VERTICES];
    QGL_INDEX_TYPE  indices[TESS_MAX_INDICES];
    GLuint          texnum[MAX_TMUS];
    int             numverts;
    int             numindices;
    int             flags;
} tesselator_t;

extern tesselator_t tess;

void GL_Flush2D(void);
void GL_DrawParticles(void);
void GL_DrawBeams(void);
void GL_DrawFlares(void);

void GL_BindArrays(glVertexArray_t va);
void GL_InitArrays(void);
void GL_ShutdownArrays(void);
void GL_LockArrays(GLsizei count);
void GL_UnlockArrays(void);

void GL_Flush3D(void);

void GL_AddAlphaFace(mface_t *face, entity_t *ent);
void GL_AddSolidFace(mface_t *face);
void GL_DrawAlphaFaces(void);
void GL_DrawSolidFaces(void);
void GL_ClearSolidFaces(void);

/*
 * gl_world.c
 *
 */
void GL_DrawBspModel(mmodel_t *model);
void GL_DrawWorld(void);
void GL_SampleLightPoint(vec3_t color);
void GL_LightPoint(const vec3_t origin, vec3_t color);

/*
 * gl_sky.c
 *
 */
void R_AddSkySurface(const mface_t *surf);
void R_ClearSkyBox(void);
void R_DrawSkyBox(void);
void R_SetSky(const char *name, float rotate, bool autorotate, const vec3_t axis);

/*
 * gl_mesh.c
 *
 */
void GL_DrawAliasModel(const model_t *model);

/*
 * hq2x.c
 *
 */
void HQ2x_Render(uint32_t *output, const uint32_t *input, int width, int height);
void HQ4x_Render(uint32_t *output, const uint32_t *input, int width, int height);
void HQ2x_Init(void);
