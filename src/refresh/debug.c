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

#include "gl.h"

#define MAX_DEBUG_LINES		1024

typedef struct debug_line_s {
	vec3_t		start, end;
	color_t		color;
	int			time; // 0 = one frame only
	float		width;
	bool		depth_test;

	struct debug_line_s	*next;
} debug_line_t;

static debug_line_t debug_lines[MAX_DEBUG_LINES];
static debug_line_t *active_lines, *free_lines;

void GL_ClearDebugLines(void)
{
    free_lines = debug_lines;
    active_lines = NULL;

	int i = 0;
    for (; i < MAX_DEBUG_LINES - 1; i++)
        debug_lines[i].next = &debug_lines[i + 1];
    debug_lines[i].next = NULL;
}

void GL_AddDebugLine(const vec3_t start, const vec3_t end, color_t color, int time, float width, bool depth_test)
{
	if (!free_lines)
		return;
	
    debug_line_t *l = free_lines;
    free_lines = l->next;
    l->next = active_lines;
    active_lines = l;

	VectorCopy(start, l->start);
	VectorCopy(end, l->end);
	l->color = color;
	l->time = time;
	l->width = width;
	l->depth_test = depth_test;
}

void GL_DrawDebugLines(void)
{
	if (!active_lines)
		return;

    GL_LoadMatrix(glr.viewmatrix);
    GL_BindTexture(0, TEXNUM_WHITE);
    GL_ArrayBits(GLA_VERTEX);
	qglEnable(GL_LINE_SMOOTH);

    GL_VertexPointer(3, 0, tess.vertices);

	GLfloat *pos_out = tess.vertices;
	tess.numverts = 0;

	int last_bits = -1;
	color_t last_color = { 0 };
	float last_width = -1;
	
    debug_line_t *active = NULL, *tail = NULL, *next = NULL;

    for (debug_line_t *l = active_lines; l; l = next) {

        next = l->next;

		int bits = GLS_BLEND_BLEND | GLS_DEPTHMASK_FALSE;

		if (!l->depth_test) {
			bits |= GLS_DEPTHTEST_DISABLE;
		}

		if (last_bits != bits ||
			last_color.u32 != l->color.u32 ||
			last_width != l->width) {

			if (tess.numverts) {
				GL_LockArrays(tess.numverts);
				qglDrawArrays(GL_LINES, 0, tess.numverts);
				GL_UnlockArrays();
			}
		
			GL_Color(l->color.u8[0] / 255.f, l->color.u8[1] / 255.f, l->color.u8[2] / 255.f, l->color.u8[3] / 255.f);
			last_color = l->color;
			qglLineWidth(l->width);
			last_width = l->width;
			GL_StateBits(bits);
			last_bits = bits;

			pos_out = tess.vertices;
			tess.numverts = 0;
		}
		
		VectorCopy(l->start, pos_out);
		VectorCopy(l->end, pos_out + 3);
		pos_out += 6;

		tess.numverts += 2;

		if (!l->time || l->time < glr.fd.time * 1000) {
            l->next = free_lines;
            free_lines = l;
		} else {
			l->next = NULL;
			if (!tail) {
				active = tail = l;
			} else {
				tail->next = l;
				tail = l;
			}
		}
	}

    active_lines = active;

	if (tess.numverts) {
		GL_LockArrays(tess.numverts);
		qglDrawArrays(GL_LINES, 0, tess.numverts);
		GL_UnlockArrays();
	}

	qglLineWidth(1.0f);
	qglDisable(GL_LINE_SMOOTH);
}

void GL_InitDebugDraw(void)
{
	GL_ClearDebugLines();
}