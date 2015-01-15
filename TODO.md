Simple-panel TODO and Guidelines
================================

##TODO

1. ~~Made it GSettings-compatible.~~ (done)
2. ~~Split any X usage to plugins.~~ (done)
3. ~~Made compositor-usable panel library.~~ (done)
4. ~~Replace menu-cache usage by gnome-menus.~~ (done to GLib AppInfos)
5. ~~Configurable volumealsa plugin.~~ (done)
6. New icon-based battery plugin.
7. ~~New run window interface (GtkEntry-based).~~(done)
8. Menubar (like gnome-panel menubar), AppMenu (like Unity, but without Ubuntu deps)
9. Replace libfm-based plugin system to similar, but internal.
10. Remove libfm dependency.
11. New GAction-based plugin system.


##Guidelines

1. Use GNOME HIG.
2. Use GActions.
3. Use GSettings.
4. Use all new GTK features (like GtkPopover or GtkModelButton)
5. Use all new GLib features (like GAction and GResource)
