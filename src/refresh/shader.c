/*
Copyright (C) 2018 Andrey Nazarov

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

#include "gl.h"
#include "common/sizebuf.h"

#define MAX_SHADER_CHARS    4096

#define GLSL(x)     SZ_Write(buf, CONST_STR_LEN(#x "\n"));
#define GLSF(x)     SZ_Write(buf, CONST_STR_LEN(x))

static void write_header(sizebuf_t *buf, glStateBits_t bits)
{
#if USE_MD5
    if (bits & GLS_MESH_MD5 && gl_config.caps & QGL_CAP_SHADER_STORAGE) {
        if (gl_config.ver_es)
            GLSF("#version 310 es\n");
        else
            GLSF("#version 430\n");
    } else
#endif
    if (gl_config.ver_es) {
        GLSF("#version 300 es\n");
    } else if (gl_config.ver_sl >= QGL_VER(1, 40)) {
        GLSF("#version 140\n");
    } else {
        GLSF("#version 130\n");
        GLSF("#extension GL_ARB_uniform_buffer_object : require\n");
    }
}

static void write_block(sizebuf_t *buf, glStateBits_t bits)
{
    GLSF("layout(std140) uniform u_block {\n");
    GLSL(mat4 m_vp;);
    GLSL(mat4 m_model;);

    if (bits & GLS_MESH_ANY) {
        GLSL(
            vec3 u_old_scale;
            vec3 u_new_scale;
            vec3 u_translate;
            vec3 u_shadedir;
            vec4 u_color;
            vec4 pad_0;
            float pad_1;
            float pad_2;
            float pad_3;
            uint u_weight_ofs;
            uint u_jointnum_ofs;
            float u_shellscale;
            float u_backlerp;
            float u_frontlerp;
        )
    } else {
        GLSL(mat4 m_sky[2];)
    }

    GLSL(
        float u_time;
        float u_modulate;
        float u_add;
        float u_intensity;
        float u_intensity2;
        float u_fog_sky_factor;
        vec2 w_amp;
        vec2 w_phase;
        vec2 u_scroll;
        vec4 u_fog_color;
        vec4 u_heightfog_start;
        vec4 u_heightfog_end;
        float u_heightfog_density;
        float u_heightfog_falloff;
        vec2 pad_4;
        vec3 u_vieworg;
    )
    GLSF("};\n");
}

static void write_shadedot(sizebuf_t *buf)
{
    GLSL(
        float shadedot(vec3 normal) {
            float d = dot(normal, u_shadedir);
            if (d < 0.0)
                d *= 0.3;
            return d + 1.0;
        }
    )
}

#if USE_MD5
static void write_skel_shader(sizebuf_t *buf, glStateBits_t bits)
{
    GLSL(
        struct Joint {
            vec4 pos;
            mat3x3 axis;
        };
        layout(std140) uniform Skeleton {
            Joint u_joints[256];
        };
    )

    if (gl_config.caps & QGL_CAP_SHADER_STORAGE) {
        GLSL(
            layout(std430, binding = 0) readonly buffer Weights {
                vec4 b_weights[];
            };

            layout(std430, binding = 1) readonly buffer JointNums {
                uint b_jointnums[];
            };
        )
    } else {
        GLSL(
            uniform samplerBuffer u_weights;
            uniform usamplerBuffer u_jointnums;
        )
    }

    GLSL(
        in vec2 a_tc;
        in vec3 a_norm;
        in uvec2 a_vert;

        out vec2 v_tc;
        out vec4 v_color;
    )

    if (bits & GLS_FOG_HEIGHT)
        GLSL(out vec3 v_world_pos;)

    if (bits & GLS_MESH_SHADE)
        write_shadedot(buf);

    GLSF("void main() {\n");
    GLSL(
        vec3 out_pos = vec3(0.0);
        vec3 out_norm = vec3(0.0);

        uint start = a_vert[0];
        uint count = a_vert[1];
    )

    GLSF("for (uint i = start; i < start + count; i++) {\n");
        if (gl_config.caps & QGL_CAP_SHADER_STORAGE) {
            GLSL(
                uint jointnum = b_jointnums[i / 4U];
                jointnum >>= (i & 3U) * 8U;
                jointnum &= 255U;

                vec4 weight = b_weights[i];
            )
        } else {
            GLSL(
                uint jointnum = texelFetch(u_jointnums, int(u_jointnum_ofs + i)).r;
                vec4 weight   = texelFetch(u_weights,   int(u_weight_ofs   + i));
            )
        }
        GLSL(
            Joint joint = u_joints[jointnum];

            vec3 wv = joint.pos.xyz + (weight.xyz * joint.axis) * joint.pos.w;
            out_pos += wv * weight.w;

            out_norm += a_norm * joint.axis * weight.w;
        )
    GLSF("}\n");

    GLSL(v_tc = a_tc;)

    if (bits & GLS_MESH_SHADE)
        GLSL(v_color = vec4(u_color.rgb * shadedot(out_norm), u_color.a);)
    else
        GLSL(v_color = u_color;)

    if (bits & GLS_MESH_SHELL)
        GLSL(out_pos += out_norm * u_shellscale;)

    if (bits & GLS_FOG_HEIGHT)
        GLSL(v_world_pos = (m_model * vec4(out_pos, 1.0)).xyz;)

    GLSL(gl_Position = m_vp * vec4(out_pos, 1.0);)
    GLSF("}\n");
}
#endif

static void write_getnormal(sizebuf_t *buf)
{
    GLSL(
        vec3 get_normal(int norm) {
            const float pi = 3.14159265358979323846;
            const float scale = pi * (2.0 / 255.0);
            float lat = float( uint(norm)       & 255U) * scale;
            float lng = float((uint(norm) >> 8) & 255U) * scale;
            return vec3(
                sin(lat) * cos(lng),
                sin(lat) * sin(lng),
                cos(lat)
            );
        }
    )
}

static void write_mesh_shader(sizebuf_t *buf, glStateBits_t bits)
{
    GLSL(
        in vec2 a_tc;
        in ivec4 a_new_pos;
    )

    if (bits & GLS_MESH_LERP)
        GLSL(in ivec4 a_old_pos;)

    GLSL(
        out vec2 v_tc;
        out vec4 v_color;
    )

    if (bits & GLS_FOG_HEIGHT)
        GLSL(out vec3 v_world_pos;)

    if (bits & (GLS_MESH_SHELL | GLS_MESH_SHADE))
        write_getnormal(buf);

    if (bits & GLS_MESH_SHADE)
        write_shadedot(buf);

    GLSF("void main() {\n");
    GLSL(v_tc = a_tc;)

    if (bits & GLS_MESH_LERP) {
        if (bits & (GLS_MESH_SHELL | GLS_MESH_SHADE))
            GLSL(
                vec3 old_norm = get_normal(a_old_pos.w);
                vec3 new_norm = get_normal(a_new_pos.w);
            )

        GLSL(vec3 pos = vec3(a_old_pos.xyz) * u_old_scale + vec3(a_new_pos.xyz) * u_new_scale + u_translate;)

        if (bits & GLS_MESH_SHELL)
            GLSL(pos += normalize((old_norm * u_backlerp + new_norm * u_frontlerp)) * u_shellscale;)

        if (bits & GLS_MESH_SHADE)
            GLSL(v_color = vec4(u_color.rgb * (shadedot(old_norm) * u_backlerp + shadedot(new_norm) * u_frontlerp), u_color.a);)
        else
            GLSL(v_color = u_color;)
    } else {
        if (bits & (GLS_MESH_SHELL | GLS_MESH_SHADE))
            GLSL(vec3 norm = get_normal(a_new_pos.w);)

        GLSL(vec3 pos = vec3(a_new_pos.xyz) * u_new_scale + u_translate;)

        if (bits & GLS_MESH_SHELL)
            GLSL(pos += norm * u_shellscale;)

        if (bits & GLS_MESH_SHADE)
            GLSL(v_color = vec4(u_color.rgb * shadedot(norm), u_color.a);)
        else
            GLSL(v_color = u_color;)
    }

    if (bits & GLS_FOG_HEIGHT)
        GLSL(v_world_pos = (m_model * vec4(pos, 1.0)).xyz;)

    GLSL(gl_Position = m_vp * vec4(pos, 1.0);)
    GLSF("}\n");
}

static void write_vertex_shader(sizebuf_t *buf, glStateBits_t bits)
{
    write_header(buf, bits);
    write_block(buf, bits);

#if USE_MD5
    if (bits & GLS_MESH_MD5) {
        write_skel_shader(buf, bits);
        return;
    }
#endif

    if (bits & GLS_MESH_MD2) {
        write_mesh_shader(buf, bits);
        return;
    }

    GLSL(in vec4 a_pos;)
    if (bits & GLS_SKY_MASK) {
        GLSL(out vec3 v_dir;)
    } else {
        GLSL(in vec2 a_tc;)
        GLSL(out vec2 v_tc;)
    }

    if (bits & GLS_LIGHTMAP_ENABLE) {
        GLSL(in vec2 a_lmtc;)
        GLSL(out vec2 v_lmtc;)
    }

    if (!(bits & GLS_TEXTURE_REPLACE)) {
        GLSL(in vec4 a_color;)
        GLSL(out vec4 v_color;)
    }

    if (bits & GLS_FOG_HEIGHT)
        GLSL(out vec3 v_world_pos;)

    GLSF("void main() {\n");
        if (bits & GLS_CLASSIC_SKY) {
            GLSL(v_dir = (m_sky[1] * a_pos).xyz;)
        } else if (bits & GLS_DEFAULT_SKY) {
            GLSL(v_dir = (m_sky[0] * a_pos).xyz;)
        } else if (bits & GLS_SCROLL_ENABLE) {
            GLSL(v_tc = a_tc + u_scroll;)
        } else {
            GLSL(v_tc = a_tc;)
        }

        if (bits & GLS_LIGHTMAP_ENABLE)
            GLSL(v_lmtc = a_lmtc;)

        if (!(bits & GLS_TEXTURE_REPLACE))
            GLSL(v_color = a_color;)

        if (bits & GLS_FOG_HEIGHT)
            GLSL(v_world_pos = (m_model * a_pos).xyz;)

        GLSL(gl_Position = m_vp * a_pos;)
    GLSF("}\n");
}

// XXX: this is very broken. but that's how it is in re-release.
static void write_height_fog(sizebuf_t *buf)
{
    GLSL({
        float dir_z = normalize(v_world_pos - u_vieworg).z;
        float eye = u_vieworg.z - u_heightfog_start.w;
        float pos = v_world_pos.z - u_heightfog_start.w;
        float density = (exp(-u_heightfog_falloff * eye) -
                         exp(-u_heightfog_falloff * pos)) / (u_heightfog_falloff * dir_z);
        float extinction = 1.0 - clamp(exp(-density), 0.0, 1.0);
        float fraction = clamp((pos - u_heightfog_start.w) / (u_heightfog_end.w - u_heightfog_start.w), 0.0, 1.0);
        vec3 fog_color = mix(u_heightfog_start.rgb, u_heightfog_end.rgb, fraction) * extinction;
        float fog = (1.0 - exp(-(u_heightfog_density * frag_depth))) * extinction;
        diffuse.rgb = mix(diffuse.rgb, fog_color.rgb, fog);
    })
}

static void write_fragment_shader(sizebuf_t *buf, glStateBits_t bits)
{
    write_header(buf, bits);

    if (gl_config.ver_es)
        GLSL(precision mediump float;)

    if (bits & GLS_UNIFORM_MASK)
        write_block(buf, bits);

    if (bits & GLS_CLASSIC_SKY) {
        GLSL(
            uniform sampler2D u_texture1;
            uniform sampler2D u_texture2;
        )
    } else if (bits & GLS_DEFAULT_SKY) {
        GLSL(uniform samplerCube u_texture;)
    } else {
        GLSL(uniform sampler2D u_texture;)
    }

    if (bits & GLS_SKY_MASK)
        GLSL(in vec3 v_dir;)
    else
        GLSL(in vec2 v_tc;)

    if (bits & GLS_LIGHTMAP_ENABLE) {
        GLSL(uniform sampler2D u_lightmap;)
        GLSL(in vec2 v_lmtc;)
    }

    if (bits & GLS_GLOWMAP_ENABLE)
        GLSL(uniform sampler2D u_glowmap;)

    if (!(bits & GLS_TEXTURE_REPLACE))
        GLSL(in vec4 v_color;)

    GLSL(out vec4 o_color;)

    if (bits & GLS_FOG_HEIGHT)
        GLSL(in vec3 v_world_pos;)

    GLSF("void main() {\n");
        if (bits & GLS_CLASSIC_SKY) {
            GLSL(
                float len = length(v_dir);
                vec2 dir = v_dir.xy * (3.0 / len);
                vec2 tc1 = dir + vec2(u_time * 0.0625);
                vec2 tc2 = dir + vec2(u_time * 0.1250);
                vec4 solid = texture(u_texture1, tc1);
                vec4 alpha = texture(u_texture2, tc2);
                vec4 diffuse = vec4((solid.rgb - alpha.rgb * 0.25) * 0.65, 1.0);
            )
        } else if (bits & GLS_DEFAULT_SKY) {
            GLSL(vec4 diffuse = texture(u_texture, v_dir);)
        } else {
            GLSL(vec2 tc = v_tc;)

            if (bits & GLS_WARP_ENABLE)
                GLSL(tc += w_amp * sin(tc.ts * w_phase + u_time);)

            GLSL(vec4 diffuse = texture(u_texture, tc);)
        }

        if (bits & GLS_ALPHATEST_ENABLE)
            GLSL(if (diffuse.a <= 0.666) discard;)

        if (bits & GLS_LIGHTMAP_ENABLE) {
            GLSL(vec4 lightmap = texture(u_lightmap, v_lmtc);)

            if (bits & GLS_GLOWMAP_ENABLE) {
                GLSL(vec4 glowmap = texture(u_glowmap, tc);)
                GLSL(lightmap.rgb = mix(lightmap.rgb, vec3(1.0), glowmap.a);)
            }

            GLSL(diffuse.rgb *= (lightmap.rgb + u_add) * u_modulate;)
        }

        if (bits & GLS_INTENSITY_ENABLE)
            GLSL(diffuse.rgb *= u_intensity;)

        if (bits & GLS_DEFAULT_FLARE)
            GLSL(
                 diffuse.rgb *= (diffuse.r + diffuse.g + diffuse.b) / 3.0;
                 diffuse.rgb *= v_color.a;
            )

        if (!(bits & GLS_TEXTURE_REPLACE))
            GLSL(diffuse *= v_color;)

        if (!(bits & GLS_LIGHTMAP_ENABLE) && (bits & GLS_GLOWMAP_ENABLE)) {
            GLSL(vec4 glowmap = texture(u_glowmap, tc);)
            if (bits & GLS_INTENSITY_ENABLE)
                GLSL(diffuse.rgb += glowmap.rgb * u_intensity2;)
            else
                GLSL(diffuse.rgb += glowmap.rgb;)
        }

        if (bits & (GLS_FOG_GLOBAL | GLS_FOG_HEIGHT))
            GLSL(float frag_depth = gl_FragCoord.z / gl_FragCoord.w;)

        if (bits & GLS_FOG_GLOBAL)
            GLSL({
                float d = u_fog_color.a * frag_depth;
                float fog = 1.0f - exp(-(d * d));
                diffuse.rgb = mix(diffuse.rgb, u_fog_color.rgb, fog);
            })

        if (bits & GLS_FOG_HEIGHT)
            write_height_fog(buf);

        if (bits & GLS_FOG_SKY)
            GLSL(diffuse.rgb = mix(diffuse.rgb, u_fog_color.rgb, u_fog_sky_factor);)

        GLSL(o_color = diffuse;)
    GLSF("}\n");
}

static GLuint create_shader(GLenum type, const sizebuf_t *buf)
{
    const GLchar *data = (const GLchar *)buf->data;
    GLint size = buf->cursize;

    GLuint shader = qglCreateShader(type);
    if (!shader) {
        Com_EPrintf("Couldn't create shader\n");
        return 0;
    }

    qglShaderSource(shader, 1, &data, &size);
    qglCompileShader(shader);
    GLint status = 0;
    qglGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        char buffer[MAX_STRING_CHARS];

        buffer[0] = 0;
        qglGetShaderInfoLog(shader, sizeof(buffer), NULL, buffer);
        qglDeleteShader(shader);

        if (buffer[0])
            Com_Printf("%s", buffer);

        Com_EPrintf("Error compiling %s shader\n",
                    type == GL_VERTEX_SHADER ? "vertex" : "fragment");
        return 0;
    }

    return shader;
}

static GLuint create_and_use_program(glStateBits_t bits)
{
    char buffer[MAX_SHADER_CHARS];
    sizebuf_t sb;

    GLuint program = qglCreateProgram();
    if (!program) {
        Com_EPrintf("Couldn't create program\n");
        return 0;
    }

    SZ_Init(&sb, buffer, sizeof(buffer), "GLSL");
    write_vertex_shader(&sb, bits);
    GLuint shader_v = create_shader(GL_VERTEX_SHADER, &sb);
    if (!shader_v)
        return program;

    SZ_Clear(&sb);
    write_fragment_shader(&sb, bits);
    GLuint shader_f = create_shader(GL_FRAGMENT_SHADER, &sb);
    if (!shader_f) {
        qglDeleteShader(shader_v);
        return program;
    }

    qglAttachShader(program, shader_v);
    qglAttachShader(program, shader_f);

#if USE_MD5
    if (bits & GLS_MESH_MD5) {
        qglBindAttribLocation(program, VERT_ATTR_MESH_TC, "a_tc");
        qglBindAttribLocation(program, VERT_ATTR_MESH_NORM, "a_norm");
        qglBindAttribLocation(program, VERT_ATTR_MESH_VERT, "a_vert");
    } else
#endif
    if (bits & GLS_MESH_MD2) {
        qglBindAttribLocation(program, VERT_ATTR_MESH_TC, "a_tc");
        if (bits & GLS_MESH_LERP)
            qglBindAttribLocation(program, VERT_ATTR_MESH_OLD_POS, "a_old_pos");
        qglBindAttribLocation(program, VERT_ATTR_MESH_NEW_POS, "a_new_pos");
    } else {
        qglBindAttribLocation(program, VERT_ATTR_POS, "a_pos");
        if (!(bits & GLS_SKY_MASK))
            qglBindAttribLocation(program, VERT_ATTR_TC, "a_tc");
        if (bits & GLS_LIGHTMAP_ENABLE)
            qglBindAttribLocation(program, VERT_ATTR_LMTC, "a_lmtc");
        if (!(bits & GLS_TEXTURE_REPLACE))
            qglBindAttribLocation(program, VERT_ATTR_COLOR, "a_color");
    }

    qglLinkProgram(program);

    qglDeleteShader(shader_v);
    qglDeleteShader(shader_f);

    GLint status = 0;
    qglGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
        char buffer[MAX_STRING_CHARS];

        buffer[0] = 0;
        qglGetProgramInfoLog(program, sizeof(buffer), NULL, buffer);

        if (buffer[0])
            Com_Printf("%s", buffer);

        Com_EPrintf("Error linking program\n");
        return program;
    }

    GLuint index = qglGetUniformBlockIndex(program, "u_block");
    if (index == GL_INVALID_INDEX) {
        Com_EPrintf("Uniform block not found\n");
        return program;
    }

    GLint size = 0;
    qglGetActiveUniformBlockiv(program, index, GL_UNIFORM_BLOCK_DATA_SIZE, &size);
    if (size != sizeof(gls.u_block)) {
        Com_EPrintf("Uniform block size mismatch: %d != %zu\n", size, sizeof(gls.u_block));
        return program;
    }

    qglUniformBlockBinding(program, index, UBO_UNIFORMS);

#if USE_MD5
    if (bits & GLS_MESH_MD5) {
        index = qglGetUniformBlockIndex(program, "Skeleton");
        if (index == GL_INVALID_INDEX) {
            Com_EPrintf("Skeleton block not found\n");
            return program;
        }
        qglUniformBlockBinding(program, index, UBO_SKELETON);
    }
#endif

    qglUseProgram(program);

#if USE_MD5
    if (bits & GLS_MESH_MD5 && !(gl_config.caps & QGL_CAP_SHADER_STORAGE)) {
        qglUniform1i(qglGetUniformLocation(program, "u_weights"), TMU_SKEL_WEIGHTS);
        qglUniform1i(qglGetUniformLocation(program, "u_jointnums"), TMU_SKEL_JOINTNUMS);
    }
#endif
    if (bits & GLS_CLASSIC_SKY) {
        qglUniform1i(qglGetUniformLocation(program, "u_texture1"), TMU_TEXTURE);
        qglUniform1i(qglGetUniformLocation(program, "u_texture2"), TMU_LIGHTMAP);
    } else {
        qglUniform1i(qglGetUniformLocation(program, "u_texture"), TMU_TEXTURE);
    }
    if (bits & GLS_LIGHTMAP_ENABLE)
        qglUniform1i(qglGetUniformLocation(program, "u_lightmap"), TMU_LIGHTMAP);
    if (bits & GLS_GLOWMAP_ENABLE)
        qglUniform1i(qglGetUniformLocation(program, "u_glowmap"), TMU_GLOWMAP);

    return program;
}

static void shader_use_program(glStateBits_t key)
{
    GLuint *prog = HashMap_Lookup(GLuint, gl_static.programs, &key);

    if (prog) {
        qglUseProgram(*prog);
    } else {
        GLuint val = create_and_use_program(key);
        HashMap_Insert(gl_static.programs, &key, &val);
    }
}

static void shader_state_bits(glStateBits_t bits)
{
    glStateBits_t diff = bits ^ gls.state_bits;

    if (diff & GLS_COMMON_MASK)
        GL_CommonStateBits(bits);

    if (diff & GLS_SHADER_MASK)
        shader_use_program(bits & GLS_SHADER_MASK);

    if (diff & GLS_SCROLL_MASK && bits & GLS_SCROLL_ENABLE) {
        GL_ScrollPos(gls.u_block.scroll, bits);
        gls.u_block_dirty = true;
    }
}

static void shader_array_bits(glArrayBits_t bits)
{
    glArrayBits_t diff = bits ^ gls.array_bits;

    for (int i = 0; i < VERT_ATTR_COUNT; i++) {
        if (!(diff & BIT(i)))
            continue;
        if (bits & BIT(i))
            qglEnableVertexAttribArray(i);
        else
            qglDisableVertexAttribArray(i);
    }
}

static void shader_array_pointers(const glVaDesc_t *desc, const GLfloat *ptr)
{
    uintptr_t base = (uintptr_t)ptr;

    for (int i = 0; i < VERT_ATTR_COUNT; i++) {
        const glVaDesc_t *d = &desc[i];
        if (d->size) {
            const GLenum type = d->type ? GL_UNSIGNED_BYTE : GL_FLOAT;
            qglVertexAttribPointer(i, d->size, type, d->type, d->stride, (void *)(base + d->offset));
        }
    }
}

static void shader_tex_coord_pointer(const GLfloat *ptr)
{
    qglVertexAttribPointer(VERT_ATTR_TC, 2, GL_FLOAT, GL_FALSE, 0, ptr);
}

static void shader_color(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    qglVertexAttrib4f(VERT_ATTR_COLOR, r, g, b, a);
}

static void shader_load_uniforms(void)
{
    GL_BindBuffer(GL_UNIFORM_BUFFER, gl_static.uniform_buffer);
    qglBufferData(GL_UNIFORM_BUFFER, sizeof(gls.u_block), &gls.u_block, GL_DYNAMIC_DRAW);
    c.uniformUploads++;
}

static void shader_load_matrix(GLenum mode, const GLfloat *matrix)
{
    switch (mode) {
    case GL_MODELVIEW:
        memcpy(gls.view_matrix, matrix, sizeof(gls.view_matrix));
        break;
    case GL_PROJECTION:
        memcpy(gls.proj_matrix, matrix, sizeof(gls.proj_matrix));
        break;
    default:
        Q_assert(!"bad mode");
    }

    GL_MultMatrix(gls.u_block.m_vp, gls.proj_matrix, gls.view_matrix);
    gls.u_block_dirty = true;
}

static void shader_setup_2d(void)
{
    gls.u_block.time = glr.fd.time;
    gls.u_block.modulate = 1.0f;
    gls.u_block.add = 0.0f;
    gls.u_block.intensity = 1.0f;
    gls.u_block.intensity2 = 1.0f;

    gls.u_block.w_amp[0] = 0.0025f;
    gls.u_block.w_amp[1] = 0.0025f;
    gls.u_block.w_phase[0] = M_PIf * 10;
    gls.u_block.w_phase[1] = M_PIf * 10;
}

static void shader_setup_fog(void)
{
    if (!(glr.fog_bits | glr.fog_bits_sky))
        return;

    VectorCopy(glr.fd.fog.color, gls.u_block.fog_color);
    gls.u_block.fog_color[3] = glr.fd.fog.density / 64;
    gls.u_block.fog_sky_factor = glr.fd.fog.sky_factor;

    VectorCopy(glr.fd.heightfog.start.color, gls.u_block.heightfog_start);
    gls.u_block.heightfog_start[3] = glr.fd.heightfog.start.dist;

    VectorCopy(glr.fd.heightfog.end.color, gls.u_block.heightfog_end);
    gls.u_block.heightfog_end[3] = glr.fd.heightfog.end.dist;

    gls.u_block.heightfog_density = glr.fd.heightfog.density;
    gls.u_block.heightfog_falloff = glr.fd.heightfog.falloff;
}

static void shader_setup_3d(void)
{
    gls.u_block.time = glr.fd.time;
    gls.u_block.modulate = gl_modulate->value * gl_modulate_world->value;
    gls.u_block.add = gl_brightness->value;
    gls.u_block.intensity = gl_intensity->value;
    gls.u_block.intensity2 = gl_intensity->value * gl_glowmap_intensity->value;

    gls.u_block.w_amp[0] = 0.0625f;
    gls.u_block.w_amp[1] = 0.0625f;
    gls.u_block.w_phase[0] = 4;
    gls.u_block.w_phase[1] = 4;

    shader_setup_fog();

    R_RotateForSky();

    // setup default matrices for world
    memcpy(gls.u_block.m_sky, glr.skymatrix, sizeof(gls.u_block.m_sky));
    memcpy(gls.u_block.m_model, gl_identity, sizeof(gls.u_block.m_model));

    VectorCopy(glr.fd.vieworg, gls.u_block.vieworg);
}

static void shader_disable_state(void)
{
    qglActiveTexture(GL_TEXTURE2);
    qglBindTexture(GL_TEXTURE_2D, 0);

    qglActiveTexture(GL_TEXTURE1);
    qglBindTexture(GL_TEXTURE_2D, 0);

    qglActiveTexture(GL_TEXTURE0);
    qglBindTexture(GL_TEXTURE_2D, 0);

    qglBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    for (int i = 0; i < VERT_ATTR_COUNT; i++)
        qglDisableVertexAttribArray(i);
}

static void shader_clear_state(void)
{
    shader_disable_state();
    shader_use_program(GLS_DEFAULT);
}

static void shader_init(void)
{
    gl_static.programs = HashMap_TagCreate(glStateBits_t, GLuint, HashInt32, NULL, TAG_RENDERER);

    qglGenBuffers(1, &gl_static.uniform_buffer);
    GL_BindBuffer(GL_UNIFORM_BUFFER, gl_static.uniform_buffer);
    qglBindBufferBase(GL_UNIFORM_BUFFER, UBO_UNIFORMS, gl_static.uniform_buffer);
    qglBufferData(GL_UNIFORM_BUFFER, sizeof(gls.u_block), NULL, GL_DYNAMIC_DRAW);

#if USE_MD5
    if (gl_config.caps & QGL_CAP_SKELETON_MASK) {
        qglGenBuffers(1, &gl_static.skeleton_buffer);
        GL_BindBuffer(GL_UNIFORM_BUFFER, gl_static.skeleton_buffer);
        qglBindBufferBase(GL_UNIFORM_BUFFER, UBO_SKELETON, gl_static.skeleton_buffer);

        if ((gl_config.caps & QGL_CAP_SKELETON_MASK) == QGL_CAP_BUFFER_TEXTURE)
            qglGenTextures(2, gl_static.skeleton_tex);
    }
#endif

    if (gl_config.ver_gl >= QGL_VER(3, 2))
        qglEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
}

static void shader_shutdown(void)
{
    shader_disable_state();
    qglUseProgram(0);

    if (gl_static.programs) {
        uint32_t map_size = HashMap_Size(gl_static.programs);
        for (int i = 0; i < map_size; i++) {
            GLuint *prog = HashMap_GetValue(GLuint, gl_static.programs, i);
            qglDeleteProgram(*prog);
        }
        HashMap_Destroy(gl_static.programs);
        gl_static.programs = NULL;
    }

    if (gl_static.uniform_buffer) {
        qglDeleteBuffers(1, &gl_static.uniform_buffer);
        gl_static.uniform_buffer = 0;
    }

#if USE_MD5
    if (gl_static.skeleton_buffer) {
        qglDeleteBuffers(1, &gl_static.skeleton_buffer);
        gl_static.skeleton_buffer = 0;
    }
    if (gl_static.skeleton_tex[0] || gl_static.skeleton_tex[1]) {
        qglDeleteTextures(2, gl_static.skeleton_tex);
        gl_static.skeleton_tex[0] = gl_static.skeleton_tex[1] = 0;
    }
#endif

    if (gl_config.ver_gl >= QGL_VER(3, 2))
        qglDisable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
}

const glbackend_t backend_shader = {
    .name = "GLSL",

    .init = shader_init,
    .shutdown = shader_shutdown,
    .clear_state = shader_clear_state,
    .setup_2d = shader_setup_2d,
    .setup_3d = shader_setup_3d,

    .load_matrix = shader_load_matrix,
    .load_uniforms = shader_load_uniforms,

    .state_bits = shader_state_bits,
    .array_bits = shader_array_bits,

    .array_pointers = shader_array_pointers,
    .tex_coord_pointer = shader_tex_coord_pointer,

    .color = shader_color,
};
