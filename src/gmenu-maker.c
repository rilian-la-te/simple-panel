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

#define GMENU_I_KNOW_THIS_IS_UNSTABLE
#include "gmenu-maker.h"

gchar *
get_applications_menu (void)
{
 const gchar *xdg_menu_prefx = g_getenv ("XDG_MENU_PREFIX");
 return g_strdup_printf ("%sapplications.menu",
 xdg_menu_prefx ? xdg_menu_prefx : "gnome-");
}

static void gmenu_changed_connect_cb()
{

}

static GMenu* load_gmenu_dir(GMenuTreeDirectory* dir,const gchar* tree_name, GMenu* dir_menu)
{
    GMenuTreeDirectory* root;
    if (!root)
    {
        gmenu_tree_new(tree_name!=NULL ? tree_name : get_applications_menu(),GMENU_TREE_FLAGS_SORT_DISPLAY_NAME);
        dir_menu = g_menu_new();
    }
    gmenu_tree_load_sync(GMENU_TREE(root),NULL);

}
