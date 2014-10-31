## Process this file with automake to produce Makefile.in

bin_PROGRAMS = simple-panel

simple_panel_CFLAGS = \
	-I$(top_srcdir) \
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" \
        -DPACKAGE_DATA_DIR=\""$(datadir)/simple-panel"\" \
        -DPACKAGE_UI_DIR=\""$(datadir)/simple-panel/ui"\" \
	-DPACKAGE_BIN_DIR=\""$(bindir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	$(PACKAGE_CFLAGS) \
	$(G_CAST_CHECKS)

BUILTIN_PLUGINS = $(top_builddir)/plugins/libbuiltin_plugins.a

pkglib_LTLIBRARIES = libsimplepanel.la

libsimplepanel_la_CFLAGS = $(simple_panel_CFLAGS)
libsimplepanel_la_SOURCES = \
        misc.c \
        css.c \
        configurator.c \
        dbg.c \
        icon-grid.c \
        panel.c \
        plugin.c \
        conf.c \
        app.c

libsimplepanel_la_LIBADD = \
       $(PACKAGE_LIBS)


if ENABLE_MENU_CACHE
MENU_SOURCES = menu-policy.c
endif

simple_panel_SOURCES = \
        gtk-run.c \
        main.c \
	$(MENU_SOURCES)

simple_panel_includedir = $(includedir)/simple-panel
simple_panel_include_HEADERS = \
	plugin.h \
	panel.h \
	misc.h \
	icon-grid.h \
	conf.h \
	css.h

if BUILD_PLUGIN_LOADER
DYNAMIC_FLAGS = -export-dynamic
endif
simple_panel_LDFLAGS = $(DYNAMIC_FLAGS)
simple_panel_DEPENDENCIES_EXTRA = $(BUILTIN_PLUGINS)

simple_panel_LDADD = \
		libsimplepanel.la \
		$(BUILTIN_PLUGINS) \
		$(PACKAGE_LIBS) \
		$(INTLLIBS)
##		$(X11_LIBS) \

EXTRA_DIST = \
	css.h \
	dbg.h \
	menu-policy.h \
	private.h \
	app.h

$(top_builddir)/plugins/libbuiltin_plugins.a: $(simple_panel_include_HEADERS)
	cd $(top_builddir)/plugins && $(MAKE) libbuiltin_plugins.a
