/*
 * Verda Ŝtelo - An anagram game in Esperanto for the web
 * Copyright (C) 2021  Neil Roberts
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

#include "vsx-game-painter.h"

#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "vsx-painter-toolbox.h"
#include "vsx-tile-painter.h"
#include "vsx-gl.h"
#include "vsx-board.h"

struct vsx_game_painter {
        struct vsx_painter_toolbox toolbox;
        bool shader_data_inited;

        struct vsx_tile_painter *tile_painter;
};

static bool
init_toolbox(struct vsx_game_painter *painter,
             struct vsx_asset_manager *asset_manager,
             struct vsx_error **error)
{
        struct vsx_painter_toolbox *toolbox = &painter->toolbox;

        if (!vsx_shader_data_init(&toolbox->shader_data,
                                  asset_manager,
                                  error))
                return false;

        painter->shader_data_inited = true;

        toolbox->image_loader = vsx_image_loader_new(asset_manager);

        return true;
}

static void
destroy_toolbox(struct vsx_game_painter *painter)
{
        struct vsx_painter_toolbox *toolbox = &painter->toolbox;

        if (toolbox->image_loader)
                vsx_image_loader_free(toolbox->image_loader);

        if (painter->shader_data_inited)
                vsx_shader_data_destroy(&toolbox->shader_data);
}

struct vsx_game_painter *
vsx_game_painter_new(struct vsx_asset_manager *asset_manager,
                     struct vsx_error **error)
{
        struct vsx_game_painter *painter = vsx_calloc(sizeof *painter);

        if (!init_toolbox(painter, asset_manager, error))
                goto error;

        painter->tile_painter = vsx_tile_painter_new(&painter->toolbox);

        return painter;

error:
        vsx_game_painter_free(painter);
        return NULL;
}

static void
fit_board_normal(struct vsx_paint_state *paint_state,
                 float scale)
{
        paint_state->board_matrix[0] =
                scale * 2.0f / paint_state->width;
        paint_state->board_matrix[1] = 0.0f;
        paint_state->board_matrix[2] = 0.0f;
        paint_state->board_matrix[3] =
                -scale * 2.0f / paint_state->height;
        paint_state->board_translation[0] =
                -VSX_BOARD_WIDTH / 2.0f * paint_state->board_matrix[0];
        paint_state->board_translation[1] =
                -VSX_BOARD_HEIGHT / 2.0f * paint_state->board_matrix[3];
}

static void
fit_board_rotated(struct vsx_paint_state *paint_state,
                 float scale)
{
        paint_state->board_matrix[0] = 0.0f;
        paint_state->board_matrix[1] =
                -scale * 2.0f / paint_state->height;
        paint_state->board_matrix[2] =
                scale * 2.0f / paint_state->width;
        paint_state->board_matrix[3] = 0.0f;
        paint_state->board_translation[0] =
                -VSX_BOARD_HEIGHT / 2.0f * paint_state->board_matrix[2];
        paint_state->board_translation[1] =
                -VSX_BOARD_WIDTH / 2.0f * paint_state->board_matrix[1];
}

static void
calculate_paint_state(struct vsx_paint_state *paint_state,
                      int fb_width,
                      int fb_height)
{
        paint_state->width = fb_width;
        paint_state->height = fb_height;

        if (fb_width == 0 || fb_height == 0) {
                memset(paint_state->board_matrix,
                       0,
                       sizeof paint_state->board_matrix);
                memset(paint_state->board_translation,
                       0,
                       sizeof paint_state->board_translation);
                return;
        }

        int large_axis, small_axis;
        bool rotate;

        if (fb_width > fb_height) {
                large_axis = fb_width;
                small_axis = fb_height;
                rotate = false;
        } else {
                large_axis = fb_height;
                small_axis = fb_width;
                rotate = true;
        }

        /* We want to know if the (possibly rotated) framebuffer
         * width/height ratio is greater than the board width/height
         * ratio. Otherwise we will fit the board so that the width
         * fills the screen instead of the height.
         *
         * (a/b > c/d) == (a*d/b*d > c*b/b*d) == (a*d > c*b)
         */
        bool fit_small = (large_axis * VSX_BOARD_HEIGHT >
                          VSX_BOARD_WIDTH * small_axis);

        float scale = (fit_small ?
                       small_axis / (float) VSX_BOARD_HEIGHT :
                       large_axis / (float) VSX_BOARD_WIDTH);

        if (rotate)
                fit_board_rotated(paint_state, scale);
        else
                fit_board_normal(paint_state, scale);
}

void
vsx_game_painter_paint(struct vsx_game_painter *painter,
                       struct vsx_game_state *game_state,
                       int width,
                       int height)
{
        vsx_gl.glViewport(0, 0, width, height);

        vsx_gl.glClear(GL_COLOR_BUFFER_BIT);

        struct vsx_paint_state paint_state;

        calculate_paint_state(&paint_state, width, height);

        vsx_tile_painter_paint(painter->tile_painter,
                               game_state,
                               &paint_state);
}

void
vsx_game_painter_free(struct vsx_game_painter *painter)
{
        if (painter->tile_painter)
                vsx_tile_painter_free(painter->tile_painter);

        destroy_toolbox(painter);

        vsx_free(painter);
}
