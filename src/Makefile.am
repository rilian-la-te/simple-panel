## Process this file with automake to produce Makefile.in

bin_PROGRAMS = simple-panel

simple_panel_CPPFLAGS = \
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

SIMPLE_PANEL_HEADER_FILES = \
	plugin.h \
	panel.h \
	misc.h \
	icon-grid.h \
	conf-gsettings.h \
	css.h

SIMPLE_PANEL_NOINST_HEADER_FILES = \
	dbg.h \
	menu-policy.h \
	private.h \
	app.h \
	app-actions.h \
	app-misc.h

SIMPLE_PANEL_ENUM_FILES = $(SIMPLE_PANEL_HEADER_FILES)
SIMPLE_PANEL_ENUM_FILES += $(SIMPLE_PANEL_NOINST_HEADER_FILES)

panel-enum-types.h: panel-enum-types.h.template $(SIMPLE_PANEL_ENUM_FILES) $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template panel-enum-types.h.template $(SIMPLE_PANEL_ENUM_FILES)) > $@

panel-enum-types.c: panel-enum-types.c.template $(SIMPLE_PANEL_ENUM_FILES) $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template panel-enum-types.c.template $(SIMPLE_PANEL_ENUM_FILES)) > $@

simple_panel_built_sources =			\
	panel-enum-types.c	\
	panel-enum-types.h

libsimplepanel_la_CPPFLAGS = $(simple_panel_CPPFLAGS)
libsimplepanel_la_SOURCES = \
	misc.c \
	css.c \
	configurator.c \
	dbg.c \
	icon-grid.c \
	panel.c \
	plugin.c \
	conf-gsettings.c \
	panel-enum-types.c

libsimplepanel_la_LIBADD = \
       $(PACKAGE_LIBS)

libsimplepanel_la_LDFLAGS = \
       -no-undefined

if ENABLE_MENU_CACHE
MENU_SOURCES = menu-policy.c
endif

simple_panel_SOURCES = \
	gtk-run.c \
	main.c  \
	app.c \
	app-actions.c \
	app-misc.c \
	$(MENU_SOURCES)

simple_panel_includedir = $(includedir)/simple-panel
simple_panel_include_HEADERS = $(SIMPLE_PANEL_HEADER_FILES)

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

EXTRA_DIST = $(SIMPLE_PANEL_NOINST_HEADER_FILES) \
	panel-enum-types.h

BUILT_SOURCES = $(simple_panel_built_sources)
CLEANFILES = panel-enum-types.c panel-enum-types.h

$(top_builddir)/plugins/libbuiltin_plugins.a: $(simple_panel_include_HEADERS)
	cd $(top_builddir)/plugins && $(MAKE) libbuiltin_plugins.a
