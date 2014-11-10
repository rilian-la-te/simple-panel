/*
 * Copyright (c) 2014 LxDE Developers, see the file AUTHORS for details.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* This is a config file parser with API similar to one used by libconfig
   for convenience but contents of the file is the own config format
   therefore it is much more restricted than libconfig is.
   Strings cannot be numeric and are not quoted (similarly to INI file format).
   Groups cannot be inside other group but only inside an anonymous list.
   That anonymous list is the only list type which is supported and there
   can be only one anonymous member in any group. */

#ifndef __CONF_GSETTINGS_H__
#define __CONF_GSETTINGS_H__ 1

#include <glib.h>
#include <stdio.h>

G_BEGIN_DECLS



G_END_DECLS

#endif /* __CONF_GSETTINGS_H__ */
