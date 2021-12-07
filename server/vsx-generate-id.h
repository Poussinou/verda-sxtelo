/*
 * Verda Ŝtelo - An anagram game in Esperanto for the web
 * Copyright (C) 2020  Neil Roberts
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

#ifndef VSX_GENERATE_ID_H
#define VSX_GENERATE_ID_H

#include <stdint.h>

#include "vsx-netaddress.h"

uint64_t
vsx_generate_id(const struct vsx_netaddress *remote_address);

#endif /* VSX_GENERATE_ID_H */
