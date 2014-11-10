/**
 * Copyright (c) 2006-2014 LxDE Developers, see the file AUTHORS for details.
 * Copyright (c) 2014 Konstantin Pugin.
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

#include "app-misc.h"
#include "private.h"

inline char *_system_config_file_name(PanelApp* app,const char *dir, const char *file_name)
{
    return g_build_filename(dir, "simple-panel", app->priv->profile, file_name, NULL);
}

inline char *_user_config_file_name(PanelApp* app, const char *name1, const char *name2)
{
    return g_build_filename(g_get_user_config_dir(), "simple-panel", app->priv->profile, name1,
                            name2, NULL);
}
