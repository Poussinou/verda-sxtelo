/*
 * Verda Ŝtelo - An anagram game in Esperanto for the web
 * Copyright (C) 2022  Neil Roberts
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "vsx-language-painter.h"

#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "vsx-gl.h"
#include "vsx-array-object.h"
#include "vsx-util.h"
#include "vsx-layout.h"

struct language {
        const char *code;
        const char *name;
};

static const struct language
languages[] = {
        { .code = "en", .name = "English" },
        { .code = "fr", .name = "Français" },
        { .code = "eo", .name = "Esperanto" },
};

#define N_LANGUAGES VSX_N_ELEMENTS(languages)

struct language_button {
        struct vsx_layout *layout;
        int x;
};

struct vsx_language_painter {
        struct vsx_game_state *game_state;
        struct vsx_painter_toolbox *toolbox;

        GLuint program;
        GLint matrix_uniform;
        GLint translation_uniform;
        GLint color_uniform;

        struct language_button buttons[N_LANGUAGES];

        int layout_y;
        int button_gap;
        int total_width, total_height;

        struct vsx_array_object *vao;
        GLuint vbo;
};

struct vertex {
        int16_t x, y;
};

#define N_VERTICES 4

/* Gap in mm between buttons */
#define BUTTON_GAP 5

/* Border in mm around all the buttons */
#define BORDER 4

static void
create_buttons(struct vsx_language_painter *painter)
{
        const struct vsx_paint_state *paint_state =
                &painter->toolbox->paint_state;

        /* Convert the button measurements from mm to pixels */
        painter->button_gap = BUTTON_GAP * paint_state->dpi * 10 / 254;
        int border = BORDER * paint_state->dpi * 10 / 254;

        int x = border;
        int max_top = 0, max_bottom = 0;

        for (int i = 0; i < N_LANGUAGES; i++) {
                struct language_button *button = painter->buttons + i;

                button->layout = vsx_layout_new(painter->toolbox->font_library,
                                                &painter->toolbox->shader_data);

                vsx_layout_set_text(button->layout, languages[i].name);
                vsx_layout_set_font(button->layout, VSX_FONT_TYPE_LABEL);

                vsx_layout_prepare(button->layout);

                const struct vsx_layout_extents *extents =
                        vsx_layout_get_logical_extents(button->layout);

                if (i > 0)
                        x += painter->button_gap;

                button->x = x;

                x += roundf(extents->right);

                if (extents->top > max_top)
                        max_top = extents->top;
                if (extents->bottom > max_bottom)
                        max_bottom = extents->bottom;
        }

        painter->layout_y = border + max_top;
        painter->total_width = x + border;
        painter->total_height = painter->layout_y + max_bottom + border;
}

static void
create_buffer(struct vsx_language_painter *painter)
{
        struct vertex vertices[N_VERTICES] = {
                { 0, 0 },
                { 0, painter->total_height },
                { painter->total_width, 0 },
                { painter->total_width, painter->total_height },
        };

        vsx_gl.glGenBuffers(1, &painter->vbo);
        vsx_gl.glBindBuffer(GL_ARRAY_BUFFER, painter->vbo);
        vsx_gl.glBufferData(GL_ARRAY_BUFFER,
                            N_VERTICES * sizeof (struct vertex),
                            vertices,
                            GL_STATIC_DRAW);

        painter->vao = vsx_array_object_new();

        vsx_array_object_set_attribute(painter->vao,
                                       VSX_SHADER_DATA_ATTRIB_POSITION,
                                       2, /* size */
                                       GL_SHORT,
                                       false, /* normalized */
                                       sizeof (struct vertex),
                                       0, /* divisor */
                                       painter->vbo,
                                       offsetof(struct vertex, x));
}

static void
init_program(struct vsx_language_painter *painter,
             struct vsx_shader_data *shader_data)
{
        painter->program =
                shader_data->programs[VSX_SHADER_DATA_PROGRAM_SOLID];

        painter->matrix_uniform =
                vsx_gl.glGetUniformLocation(painter->program,
                                            "transform_matrix");
        painter->translation_uniform =
                vsx_gl.glGetUniformLocation(painter->program,
                                            "translation");

        painter->color_uniform =
                vsx_gl.glGetUniformLocation(painter->program,
                                            "color");
}

static void *
create_cb(struct vsx_game_state *game_state,
          struct vsx_painter_toolbox *toolbox)
{
        struct vsx_language_painter *painter = vsx_calloc(sizeof *painter);

        painter->game_state = game_state;
        painter->toolbox = toolbox;

        init_program(painter, &toolbox->shader_data);

        create_buttons(painter);
        create_buffer(painter);

        return painter;
}

static bool
handle_click(struct vsx_language_painter *painter,
             const struct vsx_input_event *event)
{
        struct vsx_paint_state *paint_state = &painter->toolbox->paint_state;

        int x, y;

        if (paint_state->board_rotated) {
                int top_x = (paint_state->height / 2 -
                             painter->total_width / 2);
                int top_y = (paint_state->width / 2 +
                             painter->total_height / 2);
                x = event->click.y - top_x;
                y = top_y - event->click.x;
        } else {
                int top_x = (paint_state->width / 2 -
                             painter->total_width / 2);
                int top_y = (paint_state->height / 2 -
                             painter->total_height / 2);
                x = event->click.x - top_x;
                y = event->click.y - top_y;
        }

        if (x < 0 || x >= painter->total_width ||
            y < 0 || y >= painter->total_height) {
                vsx_game_state_set_dialog(painter->game_state,
                                          VSX_DIALOG_NONE);
                return true;
        }

        int language_num;

        for (language_num = 0; language_num < N_LANGUAGES - 1; language_num++) {
                if (x < (painter->buttons[language_num + 1].x -
                         painter->button_gap / 2))
                        break;
        }

        vsx_game_state_set_language(painter->game_state,
                                    languages[language_num].code);
        vsx_game_state_set_dialog(painter->game_state,
                                  VSX_DIALOG_MENU);

        return true;
}

static bool
input_event_cb(void *painter_data,
               const struct vsx_input_event *event)
{
        struct vsx_language_painter *painter = painter_data;

        switch (event->type) {
        case VSX_INPUT_EVENT_TYPE_DRAG_START:
        case VSX_INPUT_EVENT_TYPE_DRAG:
        case VSX_INPUT_EVENT_TYPE_ZOOM_START:
        case VSX_INPUT_EVENT_TYPE_ZOOM:
                return false;

        case VSX_INPUT_EVENT_TYPE_CLICK:
                return handle_click(painter, event);
        }

        return false;
}

static void
update_uniforms(struct vsx_language_painter *painter)
{
        const struct vsx_paint_state *paint_state =
                &painter->toolbox->paint_state;

        GLfloat matrix[4];
        GLfloat tx, ty;

        if (paint_state->board_rotated) {
                matrix[0] = 0.0f;
                matrix[1] = -2.0f / paint_state->height;
                matrix[2] = -2.0f / paint_state->width;
                matrix[3] = 0.0f;

                tx = painter->total_height / (float) paint_state->width;
                ty = painter->total_width / (float) paint_state->height;
        } else {
                matrix[0] = 2.0f / paint_state->width;
                matrix[1] = 0.0f;
                matrix[2] = 0.0f;
                matrix[3] = -2.0f / paint_state->height;

                tx = -painter->total_width / (float) paint_state->width;
                ty = painter->total_height / (float) paint_state->height;
        }

        vsx_gl.glUniformMatrix2fv(painter->matrix_uniform,
                                  1, /* count */
                                  GL_FALSE, /* transpose */
                                  matrix);
        vsx_gl.glUniform2f(painter->translation_uniform, tx, ty);

        vsx_gl.glUniform3f(painter->color_uniform, 1.0f, 1.0f, 1.0f);
}

static void
paint_cb(void *painter_data)
{
        struct vsx_language_painter *painter = painter_data;

        vsx_gl.glUseProgram(painter->program);

        update_uniforms(painter);

        vsx_array_object_bind(painter->vao);

        vsx_gl.glDrawArrays(GL_TRIANGLE_STRIP, 0, N_VERTICES);

        const struct vsx_paint_state *paint_state =
                &painter->toolbox->paint_state;

        int x_off, y_off;

        if (paint_state->board_rotated) {
                x_off = paint_state->height / 2 - painter->total_width / 2;
                y_off = paint_state->width / 2 - painter->total_height / 2;
        } else {
                x_off = paint_state->width / 2 - painter->total_width / 2;
                y_off = paint_state->height / 2 - painter->total_height / 2;
        }

        for (int i = 0; i < N_LANGUAGES; i++) {
                struct language_button *button = painter->buttons + i;

                vsx_layout_paint(button->layout,
                                 &painter->toolbox->paint_state,
                                 x_off + button->x,
                                 y_off + painter->layout_y,
                                 0.0f, 0.0f, 0.0f);
        }
}

static void
free_cb(void *painter_data)
{
        struct vsx_language_painter *painter = painter_data;

        for (int i = 0; i < VSX_N_ELEMENTS(painter->buttons); i++)
                vsx_layout_free(painter->buttons[i].layout);

        if (painter->vao)
                vsx_array_object_free(painter->vao);
        if (painter->vbo)
                vsx_gl.glDeleteBuffers(1, &painter->vbo);

        vsx_free(painter);
}

const struct vsx_painter
vsx_language_painter = {
        .create_cb = create_cb,
        .paint_cb = paint_cb,
        .input_event_cb = input_event_cb,
        .free_cb = free_cb,
};