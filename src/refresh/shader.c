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

cvar_t *gl_per_pixel_lighting;

#define MAX_SHADER_CHARS    4096

#define GLSL(x)     Q_strlcat(buf, #x "\n", MAX_SHADER_CHARS);
#define GLSF(x)     Q_strlcat(buf, x, MAX_SHADER_CHARS)

enum {
    VERT_ATTR_POS,
    VERT_ATTR_TC,
    VERT_ATTR_LMTC,
    VERT_ATTR_NORMAL,
    VERT_ATTR_COLOR
};

static void upload_u_block(void);
static void upload_dlight_block(void);

static void write_header(char *buf)
{
    *buf = 0;
    if (gl_config.ver_es) {
        GLSF("#version 300 es\n");
    } else if (gl_config.ver_sl >= QGL_VER(1, 40)) {
        GLSF("#version 140\n");
    } else {
        GLSF("#version 130\n");
        GLSF("#extension GL_ARB_uniform_buffer_object : require\n");
    }
}

static void write_block(char *buf)
{
    GLSF("layout(std140) uniform u_block {\n");
    GLSL(
        mat4 m_model;
        mat4 m_view;
        mat4 m_proj;
        float u_time; float u_modulate; float u_add; float u_intensity;
        vec2 w_amp; vec2 w_phase;
        vec2 u_scroll; float fog_sky_factor; float u_intensity2;
        vec4 view_org;
        vec4 global_fog;
        vec4 height_fog_start;
        vec4 height_fog_end;
        float height_fog_falloff; float height_fog_density; int num_dlights; float pad;
    )
    GLSF("};\n");
}

static void write_dynamic_light_block(char *buf)
{
    GLSL(
        struct dlight_t
        {
            vec3    position;
            float   radius;
            vec4    color;
        };
    )
    GLSF("#define DLIGHT_CUTOFF 64\n");
    GLSF("layout(std140) uniform u_dlights {\n");
    GLSF(va("#define MAX_DLIGHTS %i\n", MAX_DLIGHTS));
    GLSL(
        dlight_t        dlights[MAX_DLIGHTS];
    )
    GLSF("};\n");
}

static void write_vertex_shader(char *buf, GLbitfield bits)
{
    write_header(buf);
    write_block(buf);
    GLSL(in vec4 a_pos;)
    GLSL(in vec2 a_tc;)
    GLSL(out vec2 v_tc;)
    if (bits & GLS_LIGHTMAP_ENABLE) {
        GLSL(in vec2 a_lmtc;)
        GLSL(out vec2 v_lmtc;)
    }
    if (!(bits & GLS_TEXTURE_REPLACE)) {
        GLSL(in vec4 a_color;)
        GLSL(out vec4 v_color;)
    }
    if (bits & GLS_CLASSIC_SKY) {
        GLSL(out vec3 v_dir;)
    }
    if (bits & GLS_FOG_ENABLE)
        GLSL(out vec3 v_wpos;)
    if (bits & (GLS_FOG_ENABLE | GLS_DYNAMIC_LIGHTS))
        GLSL(out vec3 v_world_pos;)
    if (bits & GLS_DYNAMIC_LIGHTS) {
        GLSL(in vec3 a_normal;)
        GLSL(out vec3 v_normal;)
    }
    GLSF("void main() {\n");
        GLSL(vec2 tc = a_tc;)
        if (bits & GLS_SCROLL_ENABLE)
            GLSL(tc += u_time * u_scroll;)
        GLSL(v_tc = tc;)
        if (bits & GLS_LIGHTMAP_ENABLE)
            GLSL(v_lmtc = a_lmtc;)
        if (!(bits & GLS_TEXTURE_REPLACE))
            GLSL(v_color = a_color;)
        GLSL(gl_Position = m_proj * m_view * m_model * a_pos;)
        if (bits & GLS_FOG_ENABLE)
            GLSL(v_wpos = (m_view * m_model * a_pos).xyz;)
        if (bits & (GLS_FOG_ENABLE | GLS_DYNAMIC_LIGHTS))
            GLSL(v_world_pos = (m_model * a_pos).xyz;)
        if (bits & GLS_CLASSIC_SKY) {
            GLSL(v_dir = a_pos.xyz - view_org.xyz;)
            GLSL(v_dir[2] *= 3.0f;)
        }
        if (bits & GLS_DYNAMIC_LIGHTS)
            GLSL(v_normal = normalize((mat3(m_model) * a_normal).xyz);)
    GLSF("}\n");
}

static void write_fragment_shader(char *buf, GLbitfield bits)
{
    write_header(buf);

    if (gl_config.ver_es)
        GLSL(precision mediump float;)

    if (bits & (GLS_WARP_ENABLE | GLS_LIGHTMAP_ENABLE | GLS_INTENSITY_ENABLE | GLS_UBLOCK_MASK | GLS_DYNAMIC_LIGHTS))
        write_block(buf);

    if (bits & GLS_DYNAMIC_LIGHTS)
        write_dynamic_light_block(buf);

    GLSL(uniform sampler2D u_texture;)
    GLSL(in vec2 v_tc;)

    if (bits & GLS_LIGHTMAP_ENABLE) {
        GLSL(uniform sampler2D u_lightmap;)
        GLSL(in vec2 v_lmtc;)
    }

    if (bits & GLS_CLASSIC_SKY) {
        GLSL(uniform sampler2D u_alphamap;)
        GLSL(in vec3 v_dir;)
    }

    if (bits & GLS_GLOWMAP_ENABLE)
        GLSL(uniform sampler2D u_glowmap;)

    if (!(bits & GLS_TEXTURE_REPLACE))
        GLSL(in vec4 v_color;)

    if (bits & GLS_FOG_ENABLE)
        GLSL(in vec3 v_wpos;);

    if (bits & (GLS_FOG_ENABLE | GLS_DYNAMIC_LIGHTS))
        GLSL(in vec3 v_world_pos;)

    if (bits & GLS_DYNAMIC_LIGHTS)
        GLSL(in vec3 v_normal;)

    GLSL(out vec4 o_color;)

    if (bits & GLS_DYNAMIC_LIGHTS)
        GLSL(vec3 calc_dynamic_lights() {
            vec3 shade = vec3(0);

            for (int i = 0; i < num_dlights; i++) {
                vec3 dir = (dlights[i].position + (v_normal * 16)) - v_world_pos;
                float len = length(dir);
                float dist = max((dlights[i].radius - DLIGHT_CUTOFF - len), 0.0f);

                dir /= max(len, 1.0f);
                float lambert = max(0.0f, dot(dir, v_normal));
                shade += dlights[i].color.rgb * dist * lambert;
            }

            return shade;
        })

    GLSF("void main() {\n");
        GLSL(vec2 tc = v_tc;)

        if (bits & GLS_WARP_ENABLE)
            GLSL(tc += w_amp * sin(tc.ts * w_phase + u_time);)

        if (bits & GLS_CLASSIC_SKY) {
            GLSL(float len = length(v_dir);)
            GLSL(len = 6.f * (63.f / len);)
            GLSL(vec2 dir = v_dir.xy * len;)
            GLSL(float speed = u_time * 8.0f;)
            GLSL(tc = (dir + vec2(speed)) / 128.f;)
        }

        GLSL(vec4 diffuse = texture(u_texture, tc);)

        if (bits & GLS_CLASSIC_SKY) {
            GLSL(diffuse.rgb *= 0.65f;)
            GLSL(diffuse.a = 1.0f;)
                
            GLSL(speed = u_time * 16.0f;)
            GLSL(tc = (dir + vec2(speed)) / 128.f;)
            GLSL(vec4 alpha_diffuse = texture(u_alphamap, tc);)
            GLSL(alpha_diffuse *= 0.65f * 0.25f;)
            GLSL(diffuse.rgb = diffuse.rgb - alpha_diffuse.rgb;)
        }

        if (bits & GLS_ALPHATEST_ENABLE)
            GLSL(if (diffuse.a <= 0.666) discard;)

        if (!(bits & GLS_TEXTURE_REPLACE))
            GLSL(vec4 color = v_color;)

        if (bits & GLS_LIGHTMAP_ENABLE) {

            GLSL(vec4 lightmap = texture(u_lightmap, v_lmtc);)

            if (bits & GLS_GLOWMAP_ENABLE) {
                GLSL(vec4 glowmap = texture(u_glowmap, tc);)
                GLSL(lightmap.rgb = mix(lightmap.rgb, vec3(1.0), glowmap.a);)
            }
  
            if (bits & GLS_DYNAMIC_LIGHTS) {
                GLSL(
                    lightmap.rgb += calc_dynamic_lights();
                )
            }

            GLSL(diffuse.rgb *= (lightmap.rgb + u_add) * u_modulate;)
        } else {

            if ((bits & GLS_DYNAMIC_LIGHTS) && !(bits & GLS_TEXTURE_REPLACE)) {
                GLSL(color.rgb += calc_dynamic_lights() * u_modulate;)
            }
        }

        if (bits & GLS_INTENSITY_ENABLE)
            GLSL(diffuse.rgb *= u_intensity;)

        if (!(bits & GLS_TEXTURE_REPLACE))
            GLSL(diffuse *= color;)

        if (!(bits & GLS_LIGHTMAP_ENABLE) && (bits & GLS_GLOWMAP_ENABLE)) {
            GLSL(vec4 glowmap = texture(u_glowmap, tc);)
            GLSL(float glow_a = glowmap.a;)
            if (bits & GLS_INTENSITY_ENABLE)
                GLSL(glow_a *= u_intensity2;)
            GLSL(diffuse.rgb += glowmap.rgb * glow_a;)
        }

        if (bits & GLS_FOG_ENABLE) {
            // global fog
            GLSL(float dist_to_camera = length(v_wpos);)
            GLSL(float fog = 1.0f - exp(-pow(global_fog.w * dist_to_camera, 2.0f)););
            GLSL(diffuse.rgb = mix(diffuse.rgb, global_fog.rgb, fog);)

            // height fog
            GLSL(if (height_fog_density > 0.0f) {)
                GLSL(float altitude = view_org.z - height_fog_start.w - 64.f;);
                GLSL(vec3 view_dir = -normalize(v_wpos - v_world_pos);)
                GLSL(float view_sign = step(0.1f, sign(view_dir.z)) * 2.0f - 1.0f;);
                GLSL(float dy = view_dir.z + (0.00001f * view_sign););
                GLSL(float frag_depth = gl_FragCoord.z / gl_FragCoord.w;);
                GLSL(float altitude_dist = altitude + frag_depth * dy;);
                GLSL(float density = (exp(-height_fog_falloff * altitude) - exp(-height_fog_falloff * altitude_dist)) / (height_fog_falloff * dy););
                GLSL(float extinction = 1.0f - clamp(exp(-density), 0.0f, 1.0f););
                GLSL(float normalized_height = clamp((altitude_dist - height_fog_start.w) / (height_fog_end.w - height_fog_start.w), 0.0f, 1.0f););
                GLSL(vec3 color = (extinction * mix(height_fog_start.rgb, height_fog_end.rgb, normalized_height)););
                GLSL(float alpha = extinction * (1.0f - exp(-(height_fog_density * frag_depth))););
                GLSL(diffuse.rgb = mix(diffuse.rgb, color, alpha););
            GLSL(})
        }
        
        if (bits & GLS_SKY_FOG) {
            GLSL(diffuse.rgb = mix(diffuse.rgb, global_fog.rgb, fog_sky_factor);)
        }

        GLSL(o_color = diffuse;)
    GLSF("}\n");
}

static GLuint create_shader(GLenum type, const char *src)
{
    GLuint shader = qglCreateShader(type);
    if (!shader) {
        Com_EPrintf("Couldn't create shader\n");
        return 0;
    }

    qglShaderSource(shader, 1, &src, NULL);
    qglCompileShader(shader);
    GLint status;
    qglGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        char buffer[MAX_STRING_CHARS] = { 0 };

        qglGetShaderInfoLog(shader, sizeof(buffer), NULL, buffer);
        qglDeleteShader(shader);

        if (buffer[0])
            Com_Printf("%s", buffer);

        Com_EPrintf("Error compiling shader\n");
        return 0;
    }

    return shader;
}

static void create_and_use_program(GLbitfield bits, uint32_t hash)
{
    char buffer[MAX_SHADER_CHARS];

    GLuint program = qglCreateProgram();
    if (!program) {
        Com_EPrintf("Couldn't create program\n");
        return;
    }

    glprogram_t *prog = Z_TagMallocz(sizeof(glprogram_t), TAG_RENDERER);
    prog->bits = bits;
    prog->id = program;
    prog->hash_next = gl_static.programs_hash[hash];
    gl_static.programs_hash[hash] = prog;

    prog->next = gl_static.programs_head;
    gl_static.programs_head = prog;

    write_vertex_shader(buffer, bits);
    GLuint shader_v = create_shader(GL_VERTEX_SHADER, buffer);
    if (!shader_v)
        return;

    write_fragment_shader(buffer, bits);
    GLuint shader_f = create_shader(GL_FRAGMENT_SHADER, buffer);
    if (!shader_f) {
        qglDeleteShader(shader_v);
        return;
    }

    qglAttachShader(program, shader_v);
    qglAttachShader(program, shader_f);

    qglBindAttribLocation(program, VERT_ATTR_POS, "a_pos");
    qglBindAttribLocation(program, VERT_ATTR_TC, "a_tc");
    if (bits & GLS_LIGHTMAP_ENABLE)
        qglBindAttribLocation(program, VERT_ATTR_LMTC, "a_lmtc");
    if (!(bits & GLS_TEXTURE_REPLACE))
        qglBindAttribLocation(program, VERT_ATTR_COLOR, "a_color");
    if (bits & GLS_DYNAMIC_LIGHTS)
        qglBindAttribLocation(program, VERT_ATTR_NORMAL, "a_normal");

    qglLinkProgram(program);

    qglDeleteShader(shader_v);
    qglDeleteShader(shader_f);

    GLint status = 0;
    qglGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status) {
        char buffer[MAX_STRING_CHARS] = { 0 };

        qglGetProgramInfoLog(program, sizeof(buffer), NULL, buffer);

        if (buffer[0])
            Com_Printf("%s", buffer);

        Com_EPrintf("Error linking program\n");
        return;
    }

    GLuint index = qglGetUniformBlockIndex(program, "u_block");
    if (index == GL_INVALID_INDEX) {
        Com_EPrintf("Uniform block not found\n");
        return;
    }

    GLint size = 0;
    qglGetActiveUniformBlockiv(program, index, GL_UNIFORM_BLOCK_DATA_SIZE, &size);
    if (size != sizeof(gls.u_block)) {
        Com_EPrintf("Uniform block size mismatch: %d != %zu\n", size, sizeof(gls.u_block));
        return;
    }

    qglUniformBlockBinding(program, index, UBLOCK_MAIN);
    
    if (bits & GLS_DYNAMIC_LIGHTS) {
        index = qglGetUniformBlockIndex(program, "u_dlights");
        if (index == GL_INVALID_INDEX) {
            Com_EPrintf("DLight uniform block not found\n");
            return;
        }

        size = 0;
        qglGetActiveUniformBlockiv(program, index, GL_UNIFORM_BLOCK_DATA_SIZE, &size);
        if (size != sizeof(gls.u_dlights)) {
            Com_EPrintf("DLight uniform block size mismatch: %d != %zu\n", size, sizeof(gls.u_dlights));
            return;
        }

        qglUniformBlockBinding(program, index, UBLOCK_DLIGHTS);
    }

    qglUseProgram(program);

    qglUniform1i(qglGetUniformLocation(program, "u_texture"), 0);
    if (bits & GLS_LIGHTMAP_ENABLE)
        qglUniform1i(qglGetUniformLocation(program, "u_lightmap"), 1);
    if (bits & GLS_CLASSIC_SKY)
        qglUniform1i(qglGetUniformLocation(program, "u_alphamap"), 1);
    if (bits & GLS_GLOWMAP_ENABLE)
        qglUniform1i(qglGetUniformLocation(program, "u_glowmap"), 2);
}

static void find_and_use_program(GLbitfield bits)
{
    GLuint hash = bits >> GLS_SHADER_START_BIT;
    hash = HashInt32(&hash) & (PROGRAM_HASH_SIZE - 1);
    glStateBits_t shader_bits = bits & GLS_SHADER_MASK;

    glprogram_t *prog = gl_static.programs_hash[hash];

    if (prog) {
        for (; prog && prog->bits != shader_bits; prog = prog->hash_next) ;
    }

    if (prog)
        qglUseProgram(prog->id);
    else
        create_and_use_program(shader_bits, hash);
}

static void shader_state_bits(GLbitfield bits)
{
    // disable per-pixel lighting if requested
    if (!gl_per_pixel_lighting->integer) {
        bits &= ~GLS_DYNAMIC_LIGHTS;
    }

    // check if we actually need fog
    if (bits & GLS_FOG_ENABLE) {
        if (!gl_fog->integer ||
            !glr.fd.fog.global.density) {
            bits &= ~GLS_FOG_ENABLE;
        }
    } else if (bits & GLS_SKY_FOG) {
        if (!gl_fog->integer ||
            !glr.fd.fog.global.sky_factor) {
            bits &= ~GLS_SKY_FOG;
        }
    }

    GLbitfield diff = bits ^ gls.state_bits;

    if (diff & GLS_COMMON_MASK)
        GL_CommonStateBits(bits);

    if (diff & GLS_SHADER_MASK) {
        find_and_use_program(bits);
    }

    if (diff & GLS_UBLOCK_MASK) {
        if (bits & GLS_SCROLL_ENABLE) {
            GL_ScrollSpeed(gls.u_block.scroll, bits);
        }

        upload_u_block();
    }

    if (diff & GLS_DYNAMIC_LIGHTS) {
        upload_dlight_block();
    }
}

static void shader_array_bits(GLbitfield bits)
{
    GLbitfield diff = bits ^ gls.array_bits;

    if (diff & GLA_VERTEX) {
        if (bits & GLA_VERTEX) {
            qglEnableVertexAttribArray(VERT_ATTR_POS);
        } else {
            qglDisableVertexAttribArray(VERT_ATTR_POS);
        }
    }

    if (diff & GLA_TC) {
        if (bits & GLA_TC) {
            qglEnableVertexAttribArray(VERT_ATTR_TC);
        } else {
            qglDisableVertexAttribArray(VERT_ATTR_TC);
        }
    }

    if (diff & GLA_LMTC) {
        if (bits & GLA_LMTC) {
            qglEnableVertexAttribArray(VERT_ATTR_LMTC);
        } else {
            qglDisableVertexAttribArray(VERT_ATTR_LMTC);
        }
    }

    if (diff & GLA_COLOR) {
        if (bits & GLA_COLOR) {
            qglEnableVertexAttribArray(VERT_ATTR_COLOR);
        } else {
            qglDisableVertexAttribArray(VERT_ATTR_COLOR);
        }
    }

    if (diff & GLA_NORMAL) {
        if (bits & GLA_NORMAL) {
            qglEnableVertexAttribArray(VERT_ATTR_NORMAL);
        } else {
            qglDisableVertexAttribArray(VERT_ATTR_NORMAL);
        }
    }
}

static void shader_vertex_pointer(GLint size, GLsizei stride, const GLfloat *pointer)
{
    qglVertexAttribPointer(VERT_ATTR_POS, size, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * stride, pointer);
}

static void shader_tex_coord_pointer(GLint size, GLsizei stride, const GLfloat *pointer)
{
    qglVertexAttribPointer(VERT_ATTR_TC, size, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * stride, pointer);
}

static void shader_light_coord_pointer(GLint size, GLsizei stride, const GLfloat *pointer)
{
    qglVertexAttribPointer(VERT_ATTR_LMTC, size, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * stride, pointer);
}

static void shader_color_byte_pointer(GLint size, GLsizei stride, const GLubyte *pointer)
{
    qglVertexAttribPointer(VERT_ATTR_COLOR, size, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GLfloat) * stride, pointer);
}

static void shader_color_float_pointer(GLint size, GLsizei stride, const GLfloat *pointer)
{
    qglVertexAttribPointer(VERT_ATTR_COLOR, size, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * stride, pointer);
}

static void shader_color(GLfloat r, GLfloat g, GLfloat b, GLfloat a)
{
    qglVertexAttrib4f(VERT_ATTR_COLOR, r, g, b, a);
}

static void shader_normal_pointer(GLint size, GLsizei stride, const GLfloat *pointer)
{
    qglVertexAttribPointer(VERT_ATTR_NORMAL, size, GL_FLOAT, GL_FALSE, sizeof(GLfloat) * stride, pointer);
}

static void upload_u_block(void)
{
    qglBindBuffer(GL_UNIFORM_BUFFER, gl_static.u_blocks[UBLOCK_MAIN]);
    qglBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(gls.u_block), &gls.u_block);
    c.uniformUploads++;
}

static void upload_dlight_block(void)
{
    qglBindBuffer(GL_UNIFORM_BUFFER, gl_static.u_blocks[UBLOCK_DLIGHTS]);
    qglBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(gls.u_dlights.lights[0]) * gls.u_block.num_dlights, &gls.u_dlights);
    c.uniformUploads++;
}

static void shader_load_view_matrix(const GLfloat *model, const GLfloat *view)
{
    static const GLfloat identity[16] = { [0] = 1, [5] = 1, [10] = 1, [15] = 1 };
    
    if (!view)
        view = identity;
    if (!model)
        model = identity;
    
    memcpy(gls.u_block.model, model, sizeof(gls.u_block.model));
    memcpy(gls.u_block.view, view, sizeof(gls.u_block.view));
    upload_u_block();
}

static void shader_load_proj_matrix(const GLfloat *matrix)
{
    memcpy(gls.u_block.proj, matrix, sizeof(gls.u_block.proj));
    upload_u_block();
}

static void shader_setup_2d(void)
{
    gls.u_block.time = glr.fd.time;
    gls.u_block.modulate = 1.0f;
    gls.u_block.add = 0.0f;
    gls.u_block.intensity = 1.0f;
    gls.u_block.intensity2 = 1.0f;

    gls.u_block.w_amp[0] = 0.00666f;
    gls.u_block.w_amp[1] = 0.00666f;
    gls.u_block.w_phase[0] = M_PI * 10;
    gls.u_block.w_phase[1] = M_PI * 10;
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
    
    gls.u_block.global_fog[0] = glr.fd.fog.global.r;
    gls.u_block.global_fog[1] = glr.fd.fog.global.g;
    gls.u_block.global_fog[2] = glr.fd.fog.global.b;
    gls.u_block.global_fog[3] = glr.fd.fog.global.density;

    // FIXME I can't match the exact color as Kex but this is close...?
    gls.u_block.fog_sky_factor = glr.fd.fog.global.sky_factor * 1.5f;
    
    gls.u_block.height_fog_start[0] = glr.fd.fog.height.start.r;
    gls.u_block.height_fog_start[1] = glr.fd.fog.height.start.g;
    gls.u_block.height_fog_start[2] = glr.fd.fog.height.start.b;
    gls.u_block.height_fog_start[3] = glr.fd.fog.height.start.dist;
    
    gls.u_block.height_fog_end[0] = glr.fd.fog.height.end.r;
    gls.u_block.height_fog_end[1] = glr.fd.fog.height.end.g;
    gls.u_block.height_fog_end[2] = glr.fd.fog.height.end.b;
    gls.u_block.height_fog_end[3] = glr.fd.fog.height.end.dist;
    
    gls.u_block.height_fog_falloff = glr.fd.fog.height.falloff;
    gls.u_block.height_fog_density = glr.fd.fog.height.density;

    VectorCopy(glr.fd.vieworg, gls.u_block.view_org);

    if (gl_per_pixel_lighting->integer) {
        gls.u_block.num_dlights = glr.fd.num_dlights;

        for (int i = 0; i < min(q_countof(gls.u_dlights.lights), glr.fd.num_dlights); i++) {
            const dlight_t *dl = &glr.fd.dlights[i];
            VectorCopy(dl->origin, gls.u_dlights.lights[i].position);
            gls.u_dlights.lights[i].radius = dl->intensity;
            VectorScale(dl->color, (1.0f / 255), gls.u_dlights.lights[i].color);
        }
    }
}

static void shader_clear_state(void)
{
    qglActiveTexture(GL_TEXTURE2);
    qglBindTexture(GL_TEXTURE_2D, 0);

    qglActiveTexture(GL_TEXTURE1);
    qglBindTexture(GL_TEXTURE_2D, 0);

    qglActiveTexture(GL_TEXTURE0);
    qglBindTexture(GL_TEXTURE_2D, 0);

    qglDisableVertexAttribArray(VERT_ATTR_POS);
    qglDisableVertexAttribArray(VERT_ATTR_TC);
    qglDisableVertexAttribArray(VERT_ATTR_LMTC);
    qglDisableVertexAttribArray(VERT_ATTR_COLOR);
    qglDisableVertexAttribArray(VERT_ATTR_NORMAL);

    find_and_use_program(GLS_DEFAULT);
}

static void shader_init(void)
{
    qglGenBuffers(NUM_UBLOCKS, gl_static.u_blocks);

    qglBindBuffer(GL_UNIFORM_BUFFER, gl_static.u_blocks[UBLOCK_MAIN]);
    qglBindBufferBase(GL_UNIFORM_BUFFER, UBLOCK_MAIN, gl_static.u_blocks[UBLOCK_MAIN]);
    qglBufferData(GL_UNIFORM_BUFFER, sizeof(gls.u_block), NULL, GL_DYNAMIC_DRAW);

    qglBindBuffer(GL_UNIFORM_BUFFER, gl_static.u_blocks[UBLOCK_DLIGHTS]);
    qglBindBufferBase(GL_UNIFORM_BUFFER, UBLOCK_DLIGHTS, gl_static.u_blocks[UBLOCK_DLIGHTS]);
    qglBufferData(GL_UNIFORM_BUFFER, sizeof(gls.u_dlights), NULL, GL_DYNAMIC_DRAW);

    // precache common shader
    find_and_use_program(GLS_DEFAULT);

    gl_per_pixel_lighting = Cvar_Get("gl_per_pixel_lighting", "1", 0);
}

static void shader_shutdown(void)
{
    qglUseProgram(0);

    for (glprogram_t *head = gl_static.programs_head; head; ) {
        qglDeleteProgram(head->id);
        glprogram_t *next = head->next;
        Z_Free(head);
        head = next;
    }

    gl_static.programs_head = NULL;
    memset(gl_static.programs_hash, 0, sizeof(gl_static.programs_hash));

    qglBindBuffer(GL_UNIFORM_BUFFER, 0);
    qglDeleteBuffers(NUM_UBLOCKS, gl_static.u_blocks);
    memset(gl_static.u_blocks, 0, sizeof(gl_static.u_blocks));
}

static bool shader_use_dlights(void)
{
    return !!gl_per_pixel_lighting->integer;
}

const glbackend_t backend_shader = {
    .name = "GLSL",

    .init = shader_init,
    .shutdown = shader_shutdown,
    .clear_state = shader_clear_state,
    .setup_2d = shader_setup_2d,
    .setup_3d = shader_setup_3d,

    .load_proj_matrix = shader_load_proj_matrix,
    .load_view_matrix = shader_load_view_matrix,

    .state_bits = shader_state_bits,
    .array_bits = shader_array_bits,

    .vertex_pointer = shader_vertex_pointer,
    .tex_coord_pointer = shader_tex_coord_pointer,
    .light_coord_pointer = shader_light_coord_pointer,
    .color_byte_pointer = shader_color_byte_pointer,
    .color_float_pointer = shader_color_float_pointer,
    .color = shader_color,
    .normal_pointer = shader_normal_pointer,
    .use_dlights = shader_use_dlights
};
