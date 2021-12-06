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

#ifndef VSX_NETADDRESS_H
#define VSX_NETADDRESS_H

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>

struct vsx_netaddress {
        short int family;
        uint16_t port;
        union {
                /* Both in network byte order */
                struct in_addr ipv4;
                struct in6_addr ipv6;
        };
};

struct vsx_netaddress_native {
        union {
                struct sockaddr sockaddr;
                struct sockaddr_in sockaddr_in;
                struct sockaddr_in6 sockaddr_in6;
        };
        socklen_t length;
};

void
vsx_netaddress_to_native(const struct vsx_netaddress *address,
                         struct vsx_netaddress_native *native);

void
vsx_netaddress_from_native(struct vsx_netaddress *address,
                           const struct vsx_netaddress_native *native);

char *
vsx_netaddress_to_string(const struct vsx_netaddress *address);

bool
vsx_netaddress_from_string(struct vsx_netaddress *address,
                           const char *str,
                           int default_port);

#endif /* VSX_NETADDRESS_H */
