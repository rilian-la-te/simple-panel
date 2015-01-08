#Simple Panel
Simple-panel is a lightweight desktop panel (fork of LXPanel).

For roadmap of the fork, see [TODO](TODO.md).

To build this program you need some development packages:
-  *libtool 2.2* or newer, *intltool*, *libgtk 3.12* or newer,
-  *libfm-gtk 1.2* or newer.
Optional development packages may be required to build some modules:
-  *libmenu-cache*,
-  *libX11* ('deskno', 'xkb' and 'wincmd' plugins weren't built if missing),
-  *libwnck* ('pager' and 'launchtaskbar' plugins weren't built if missing),
-  *libalsasound* ('volumealsa' plugin isn't built if missing),
-  *libindicator-0.3.0* ('indicator' plugin isn't built if missing),
-  *wireless-tools* (required to build 'netstat' plugin),
-  *libxml-2.0* (required to build 'weather' plugin).

To install this program, three other packages are needed:
  *menu-cache*, *libfm-gtk-3.0*, *lxmenu-data*.
Please install them before installing simple-panel.

There are 1 program contained in the package.

simple-panel: the panel.

About Netstat and Netstatus plugins:

1. netstatus was ported from netstatus panel applet of GNOME Project.  This
   plugin has good support on Linux/BSD/other UNIX, and it is released under
   GNU GPL. (the same as LxDE)

2. netstat is a new plugin written by LxDE developers as the lightweight
   replacement of netstatus plugin.  It aims to be more usable and resource
   efficient.  At the current stage, netstat runs only on Linux.


About theming & simple-panel:

1. For transparency simple-panel requires compositing.

2. Current icon names that can be themed specifically for simple-panel include:
  -	*"simple-panel-background"
  -	*"clock"
  -	*"capslock-on"
  -	*"capslock-off"
  -	*"numlock-on"
  -	*"numlock-off"
  -	*"scrllock-on"
  -	*"scrllock-off"
  -	*"wincmd"
  -	*"ns-lock"
  -	*possibly more, as yet unfound.

3. You can also set theme for any plugin specifically using it's widget name
    which is equal to plugin type.

4. Simple-panel can load specific CSS file. CSS file must be referenced in config
    file ~/.config/simple-panel/$PROFILE/config, where $PROFILE is the profile used on
    the simple-panel start.

5. Simple-panel uses GKeyFile config format and GSettings.

There are also a substantial amount of others, but they use the icon naming specification.


About keyboard options translations in xkb plugin:

The 'xkb' plugin can use translations from language packs that are present
    in many distributions. To use it you should have language pack which
    includes "xkeyboard-config" translations.
