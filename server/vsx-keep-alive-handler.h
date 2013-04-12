/*
 * Verda Ŝtelo - An anagram game in Esperanto for the web
 * Copyright (C) 2012  Neil Roberts
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

#ifndef __VSX_KEEP_ALIVE_HANDLER_H__
#define __VSX_KEEP_ALIVE_HANDLER_H__

#include <glib-object.h>
#include <gio/gio.h>
#include "vsx-request-handler.h"
#include "vsx-person-set.h"

G_BEGIN_DECLS

#define VSX_TYPE_KEEP_ALIVE_HANDLER             \
  (vsx_keep_alive_handler_get_type())
#define VSX_KEEP_ALIVE_HANDLER(obj)                             \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj),                           \
                               VSX_TYPE_KEEP_ALIVE_HANDLER,     \
                               VsxKeepAliveHandler))
#define VSX_KEEP_ALIVE_HANDLER_CLASS(klass)                     \
  (G_TYPE_CHECK_CLASS_CAST ((klass),                            \
                            VSX_TYPE_KEEP_ALIVE_HANDLER,        \
                            VsxKeepAliveHandlerClass))
#define VSX_IS_KEEP_ALIVE_HANDLER(obj)                          \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj),                           \
                               VSX_TYPE_KEEP_ALIVE_HANDLER))
#define VSX_IS_KEEP_ALIVE_HANDLER_CLASS(klass)                  \
  (G_TYPE_CHECK_CLASS_TYPE ((klass),                            \
                            VSX_TYPE_KEEP_ALIVE_HANDLER))
#define VSX_KEEP_ALIVE_HANDLER_GET_CLASS(obj)                   \
  (G_TYPE_INSTANCE_GET_CLASS ((obj),                            \
                              VSX_KEEP_ALIVE_HANDLER,           \
                              VsxKeepAliveHandlerClass))

typedef struct _VsxKeepAliveHandler      VsxKeepAliveHandler;
typedef struct _VsxKeepAliveHandlerClass VsxKeepAliveHandlerClass;

struct _VsxKeepAliveHandlerClass
{
  VsxRequestHandlerClass parent_class;
};

struct _VsxKeepAliveHandler
{
  VsxRequestHandler parent;

  VsxPerson *person;

  VsxResponse *response;
};

GType
vsx_keep_alive_handler_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __VSX_KEEP_ALIVE_HANDLER_H__ */
