## Process this file with automake to produce Makefile.in

bin_PROGRAMS = simple-panel

simple_panel_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)/simple-panel"\" \
	-DPACKAGE_UI_DIR=\""$(datadir)/simple-panel/ui"\" \
	-DPACKAGE_BIN_DIR=\""$(bindir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	$(PACKAGE_CFLAGS) \
	$(G_CAST_CHECKS)

BUILTIN_PLUGINS = $(top_builddir)/plugins/libbuiltin_plugins.a


SIMPLE_PANEL_NOINST_HEADER_FILES = \
	private.h \
	app.h \
	app-actions.h \
	app-misc.h

simple_panel_resdeps = $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies simple-panel.gresource.xml)

simple-panel-resources.c: simple-panel.gresource.xml $(simple_panel_resdeps)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source simple-panel.gresource.xml

BUILT_SOURCES = \
	simple-panel-resources.c

nodist_simple_panel_SOURCES = simple-panel-resources.c
simple_panel_SOURCES = \
	gtk-run.c \
	main.c  \
	app.c \
	app-actions.c

simple_panel_includedir = $(includedir)/simple-panel
simple_panel_include_HEADERS = $(SIMPLE_PANEL_HEADER_FILES)

if BUILD_PLUGIN_LOADER
DYNAMIC_FLAGS = -export-dynamic
endif
simple_panel_LDFLAGS = $(DYNAMIC_FLAGS) -rpath $(pkglibdir)
simple_panel_DEPENDENCIES_EXTRA = $(BUILTIN_PLUGINS)

simple_panel_LDADD = \
		$(pkglibdir)/libsimplepanel.la \
		$(BUILTIN_PLUGINS) \
		$(PACKAGE_LIBS) \
		$(INTLLIBS)
##		$(X11_LIBS) \

EXTRA_DIST = $(SIMPLE_PANEL_NOINST_HEADER_FILES)

CLEANFILES = \
	simple-panel-resources.c \
	simple-panel.gresource.xml

$(top_builddir)/plugins/libbuiltin_plugins.a: $(simple_panel_include_HEADERS)
	cd $(top_builddir)/plugins && $(MAKE) libbuiltin_plugins.a
