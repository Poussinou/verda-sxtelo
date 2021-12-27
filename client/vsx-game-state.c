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

#include "vsx-game-state.h"

#include <pthread.h>
#include <strings.h>
#include <string.h>
#include <stdalign.h>

#include "vsx-util.h"
#include "vsx-signal.h"
#include "vsx-buffer.h"
#include "vsx-bitmask.h"
#include "vsx-list.h"
#include "vsx-slab.h"
#include "vsx-main-thread.h"

struct vsx_game_state_player {
        char *name;
        enum vsx_game_state_player_flag flags;
};

struct vsx_game_state_tile_private {
        struct vsx_game_state_tile public;
        struct vsx_list link;
};

struct queued_event {
        struct vsx_list link;
        struct vsx_connection_event event;
};

struct vsx_game_state {
        /* This data is only accessed from the main thread and doesn’t
         * need a mutex.
         */

        struct vsx_game_state_player players[VSX_GAME_STATE_N_VISIBLE_PLAYERS];

        enum vsx_game_state_shout_state shout_state;
        int shouting_player;

        int self;

        /* Array of tile pointers indexed by tile number */
        struct vsx_buffer tiles_by_index;
        /* List of tiles in reverse order of last updated */
        struct vsx_list tile_list;
        /* Slab allocator for the tiles */
        struct vsx_slab_allocator tile_allocator;

        struct vsx_worker *worker;
        struct vsx_connection *connection;
        struct vsx_listener event_listener;

        /* A counter that is incremented every time an event is
         * received and every time a command is queued. This can be
         * used to detect if a tile was updated after a command was
         * sent.
         */
        uint32_t time_counter;

        struct vsx_signal event_signal;

        pthread_mutex_t mutex;

        /* The following data is protected by the mutex and can be
         * modified from any thread.
         */

        struct vsx_list event_queue;
        struct vsx_main_thread_token *flush_queue_token;

        struct vsx_list freed_events;
};

static void
set_shout_state_for_player(struct vsx_game_state *game_state,
                           int player_num)
{
        game_state->shout_state =
                player_num == game_state->self ?
                VSX_GAME_STATE_SHOUT_STATE_SELF :
                VSX_GAME_STATE_SHOUT_STATE_OTHER;
}

static void
ensure_n_tiles(struct vsx_game_state *game_state,
               int n_tiles)
{
        size_t new_length =
                sizeof (struct vsx_game_state_tile_private *) * n_tiles;
        size_t old_length = game_state->tiles_by_index.length;

        if (new_length > old_length) {
                vsx_buffer_set_length(&game_state->tiles_by_index, new_length);
                memset(game_state->tiles_by_index.data + old_length,
                       0,
                       new_length - old_length);
        }
}

static struct vsx_game_state_tile_private *
get_tile_by_index(struct vsx_game_state *game_state,
                  int tile_num)
{
        ensure_n_tiles(game_state, tile_num + 1);

        struct vsx_game_state_tile_private **tile_pointers =
                (struct vsx_game_state_tile_private **)
                game_state->tiles_by_index.data;

        struct vsx_game_state_tile_private *tile = tile_pointers[tile_num];

        if (tile == NULL) {
                tile = vsx_slab_allocate(&game_state->tile_allocator,
                                         sizeof *tile,
                                         alignof *tile);
                tile_pointers[tile_num] = tile;
                vsx_list_insert(game_state->tile_list.prev, &tile->link);
        }

        return tile;
}

size_t
vsx_game_state_get_n_tiles(struct vsx_game_state *game_state)
{
        return (game_state->tiles_by_index.length /
                sizeof (struct vsx_game_state_tile_private *));
}

void
vsx_game_state_foreach_tile(struct vsx_game_state *game_state,
                            vsx_game_state_foreach_tile_cb cb,
                            void *user_data)
{
        struct vsx_game_state_tile_private *tile;

        vsx_list_for_each(tile, &game_state->tile_list, link) {
                cb(&tile->public, user_data);
        }
}

void
vsx_game_state_foreach_player(struct vsx_game_state *game_state,
                              vsx_game_state_foreach_player_cb cb,
                              void *user_data)
{
        for (int i = 0; i < VSX_GAME_STATE_N_VISIBLE_PLAYERS; i++) {
                struct vsx_game_state_player *player = game_state->players + i;

                cb(player->name, player->flags, user_data);
        }
}

static void
handle_header(struct vsx_game_state *game_state,
              const struct vsx_connection_event *event)
{
        game_state->self = event->header.self_num;
}

static void
handle_player_name_changed(struct vsx_game_state *game_state,
                           const struct vsx_connection_event *event)
{
        int player_num = event->player_name_changed.player_num;

        if (player_num >= VSX_GAME_STATE_N_VISIBLE_PLAYERS)
                return;

        struct vsx_game_state_player *player =
                game_state->players + player_num;

        vsx_free(player->name);
        player->name = vsx_strdup(event->player_name_changed.name);
}

static void
handle_player_flags_changed(struct vsx_game_state *game_state,
                            const struct vsx_connection_event *event)
{
        int player_num = event->player_shouting_changed.player_num;

        if (player_num >= VSX_GAME_STATE_N_VISIBLE_PLAYERS)
                return;

        struct vsx_game_state_player *player =
                game_state->players + player_num;

        /* Leave the shouting flag as it was */
        player->flags = ((player->flags & VSX_GAME_STATE_PLAYER_FLAG_SHOUTING) |
                         event->player_flags_changed.flags);
}

static void
handle_player_shouting_changed(struct vsx_game_state *game_state,
                               const struct vsx_connection_event *event)
{
        int player_num = event->player_shouting_changed.player_num;

        struct vsx_game_state_player *player =
                player_num < VSX_GAME_STATE_N_VISIBLE_PLAYERS ?
                game_state->players + player_num :
                NULL;

        if (event->player_shouting_changed.shouting) {
                if (player)
                        player->flags |= VSX_GAME_STATE_PLAYER_FLAG_SHOUTING;

                game_state->shouting_player = player_num;
                set_shout_state_for_player(game_state, player_num);
        } else {
                if (player)
                        player->flags &= ~VSX_GAME_STATE_PLAYER_FLAG_SHOUTING;

                if (player_num == game_state->shouting_player) {
                        game_state->shouting_player = -1;
                        game_state->shout_state =
                                VSX_GAME_STATE_SHOUT_STATE_NOONE;
                }
        }
}

static void
handle_tile_changed(struct vsx_game_state *game_state,
                    const struct vsx_connection_event *event)
{
        struct vsx_game_state_tile_private *state_tile =
                get_tile_by_index(game_state, event->tile_changed.num);

        state_tile->public.number = event->tile_changed.num;
        state_tile->public.x = event->tile_changed.x;
        state_tile->public.y = event->tile_changed.y;
        state_tile->public.letter = event->tile_changed.letter;
        state_tile->public.update_time = game_state->time_counter;
        state_tile->public.last_moved_by_self =
                event->tile_changed.last_player_moved == game_state->self;

        /* Move the tile to the end of the list so that the list will
         * always been in reverse order of most recently updated.
         */
        vsx_list_remove(&state_tile->link);
        vsx_list_insert(game_state->tile_list.prev, &state_tile->link);
}

static void
handle_event(struct vsx_game_state *game_state,
             const struct vsx_connection_event *event)
{
        game_state->time_counter++;

        switch (event->type) {
        case VSX_CONNECTION_EVENT_TYPE_HEADER:
                handle_header(game_state, event);
                break;
        case VSX_CONNECTION_EVENT_TYPE_PLAYER_NAME_CHANGED:
                handle_player_name_changed(game_state, event);
                break;
        case VSX_CONNECTION_EVENT_TYPE_PLAYER_FLAGS_CHANGED:
                handle_player_flags_changed(game_state, event);
                break;
        case VSX_CONNECTION_EVENT_TYPE_PLAYER_SHOUTING_CHANGED:
                handle_player_shouting_changed(game_state, event);
                break;
        case VSX_CONNECTION_EVENT_TYPE_TILE_CHANGED:
                handle_tile_changed(game_state, event);
                break;
        default:
                break;
        }
}

static void
flush_queue_cb(void *data)
{
        struct vsx_game_state *game_state = data;

        pthread_mutex_lock(&game_state->mutex);

        game_state->flush_queue_token = NULL;

        struct vsx_list event_queue;
        vsx_list_init(&event_queue);
        vsx_list_insert_list(&event_queue, &game_state->event_queue);
        vsx_list_init(&game_state->event_queue);

        pthread_mutex_unlock(&game_state->mutex);

        struct queued_event *queued_event;

        vsx_list_for_each(queued_event, &event_queue, link) {
                handle_event(game_state, &queued_event->event);
                vsx_signal_emit(&game_state->event_signal,
                                &queued_event->event);
                vsx_connection_destroy_event(&queued_event->event);
        }

        pthread_mutex_lock(&game_state->mutex);

        vsx_list_insert_list(&game_state->freed_events, &event_queue);

        pthread_mutex_unlock(&game_state->mutex);
}

static void
event_cb(struct vsx_listener *listener,
         void *data)
{
        const struct vsx_connection_event *event = data;

        /* Ignore poll_changed events because they will be frequent
         * and only interesting for the vsx_worker.
         */
        if (event->type == VSX_CONNECTION_EVENT_TYPE_POLL_CHANGED)
                return;

        struct vsx_game_state *game_state =
                vsx_container_of(listener,
                                 struct vsx_game_state,
                                 event_listener);

        pthread_mutex_lock(&game_state->mutex);

        struct queued_event *queued_event;

        if (vsx_list_empty(&game_state->freed_events)) {
                queued_event = vsx_alloc(sizeof *queued_event);
        } else {
                queued_event = vsx_container_of(game_state->freed_events.next,
                                                struct queued_event,
                                                link);
                vsx_list_remove(&queued_event->link);
        }

        vsx_connection_copy_event(&queued_event->event, event);

        vsx_list_insert(game_state->event_queue.prev, &queued_event->link);

        if (game_state->flush_queue_token == NULL) {
                game_state->flush_queue_token =
                        vsx_main_thread_queue_idle(flush_queue_cb,
                                                   game_state);
        }

        pthread_mutex_unlock(&game_state->mutex);
}

void
vsx_game_state_shout(struct vsx_game_state *game_state)
{
        game_state->time_counter++;

        vsx_worker_lock(game_state->worker);
        vsx_connection_shout(game_state->connection);
        vsx_worker_unlock(game_state->worker);
}

void
vsx_game_state_turn(struct vsx_game_state *game_state)
{
        game_state->time_counter++;

        vsx_worker_lock(game_state->worker);
        vsx_connection_turn(game_state->connection);
        vsx_worker_unlock(game_state->worker);
}

void
vsx_game_state_move_tile(struct vsx_game_state *game_state,
                         int tile_num,
                         int x, int y)
{
        game_state->time_counter++;

        vsx_worker_lock(game_state->worker);
        vsx_connection_move_tile(game_state->connection,
                                 tile_num,
                                 x, y);
        vsx_worker_unlock(game_state->worker);
}

struct vsx_game_state *
vsx_game_state_new(struct vsx_worker *worker,
                   struct vsx_connection *connection)
{
        struct vsx_game_state *game_state = vsx_calloc(sizeof *game_state);

        pthread_mutex_init(&game_state->mutex, NULL /* attr */);

        vsx_signal_init(&game_state->event_signal);

        vsx_list_init(&game_state->event_queue);
        vsx_list_init(&game_state->freed_events);

        vsx_buffer_init(&game_state->tiles_by_index);
        vsx_slab_init(&game_state->tile_allocator);
        vsx_list_init(&game_state->tile_list);

        game_state->shout_state = VSX_GAME_STATE_SHOUT_STATE_NOONE;

        game_state->worker = worker;
        game_state->connection = connection;

        vsx_worker_lock(game_state->worker);

        game_state->event_listener.notify = event_cb;
        vsx_signal_add(vsx_connection_get_event_signal(connection),
                       &game_state->event_listener);

        vsx_worker_unlock(game_state->worker);

        return game_state;
}

uint32_t
vsx_game_state_get_time_counter(struct vsx_game_state *game_state)
{
        return game_state->time_counter;
}

enum vsx_game_state_shout_state
vsx_game_state_get_shout_state(struct vsx_game_state *game_state)
{
        return game_state->shout_state;
}

struct vsx_signal *
vsx_game_state_get_event_signal(struct vsx_game_state *game_state)
{
        return &game_state->event_signal;
}

static void
free_event_queue(struct vsx_game_state *game_state)
{
        struct queued_event *queued_event, *tmp;

        vsx_list_for_each_safe(queued_event,
                               tmp,
                               &game_state->event_queue,
                               link) {
                vsx_connection_destroy_event(&queued_event->event);
                vsx_free(queued_event);
        }
}

static void
free_freed_events(struct vsx_game_state *game_state)
{
        struct queued_event *queued_event, *tmp;

        vsx_list_for_each_safe(queued_event,
                               tmp,
                               &game_state->freed_events,
                               link) {
                vsx_free(queued_event);
        }
}

void
vsx_game_state_free(struct vsx_game_state *game_state)
{
        vsx_list_remove(&game_state->event_listener.link);

        for (int i = 0; i < VSX_GAME_STATE_N_VISIBLE_PLAYERS; i++)
                vsx_free(game_state->players[i].name);

        if (game_state->flush_queue_token)
                vsx_main_thread_cancel_idle(game_state->flush_queue_token);

        free_event_queue(game_state);
        free_freed_events(game_state);

        vsx_buffer_destroy(&game_state->tiles_by_index);
        vsx_slab_destroy(&game_state->tile_allocator);

        pthread_mutex_destroy(&game_state->mutex);

        vsx_free(game_state);
}
