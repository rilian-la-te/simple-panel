## Process this file with automake to produce Makefile.in

bin_PROGRAMS = simple-panel vala-panel

simple_panel_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/libvalapanel \
	-DGETTEXT_PACKAGE=\""simple-panel"\" \
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)/simple-panel"\" \
	-DPACKAGE_UI_DIR=\""$(datadir)/simple-panel/ui"\" \
	-DPACKAGE_BIN_DIR=\""$(bindir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DG_SETTINGS_ENABLE_BACKEND \
	$(PACKAGE_CFLAGS) \
	$(G_CAST_CHECKS)

vala_panel_CPPFLAGS = $(simple_panel_CPPFLAGS)

BUILTIN_PLUGINS = $(top_builddir)/plugins/libbuiltin_plugins.a

SIMPLE_PANEL_NOINST_HEADER_FILES = \
	private.h \
	app.h \
	app-actions.h \
	app-misc.h

simple_panel_resdeps = $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies simple-panel.gresource.xml)

simple-panel-resources.c: simple-panel.gresource.xml $(simple_panel_resdeps)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source simple-panel.gresource.xml
	
vala_panel_resdeps = $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies vala-panel.gresource.xml)

vala-panel-resources.c: vala-panel.gresource.xml $(simple_panel_resdeps)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source vala-panel.gresource.xml

BUILT_SOURCES = \
	simple-panel-resources.c \
	vala-panel-resources.c

nodist_simple_panel_SOURCES = simple-panel-resources.c
simple_panel_SOURCES = \
	gtk-run.c \
	main.c  \
	app.c \
	app-actions.c

nodist_vala_panel_SOURCES = vala-panel-resources.c
vala_panel_SOURCES = runner.vala application.vala

vala_panel_VALAFLAGS = \
	--vapidir=$(top_srcdir)/vapi \
	--vapidir=. \
	--target-glib=2.40 \
	--gresources=$(srcdir)/vala-panel.gresource.xml \
	--pkg gtk+-3.0 \
	--pkg gio-2.0 \
	--pkg gio-unix-2.0 \
	--pkg config \
	--pkg temp \
	--pkg vala-panel \
	--pkg gio-settingsbackend-2.0 \
	--pkg posix \
	$(VALAFLAGS)

simple_panel_includedir = $(includedir)/simple-panel
simple_panel_include_HEADERS = $(SIMPLE_PANEL_HEADER_FILES)

if BUILD_PLUGIN_LOADER
DYNAMIC_FLAGS = -export-dynamic
endif
simple_panel_LDFLAGS = $(DYNAMIC_FLAGS) -rpath $(pkglibdir)
simple_panel_DEPENDENCIES_EXTRA = $(BUILTIN_PLUGINS)

vala_panel_LDFLAGS = $(DYNAMIC_FLAGS) -rpath $(pkglibdir)
vala_panel_DEPENDENCIES_EXTRA = $(BUILTIN_PLUGINS)

simple_panel_LDADD = \
		$(top_builddir)/lib/libsimplepanel.la \
		$(top_builddir)/libvalapanel/libvalapanel.la \
		$(BUILTIN_PLUGINS) \
		$(PACKAGE_LIBS) \
		$(INTLLIBS)
##		$(X11_LIBS) \

vala_panel_LDADD = $(simple_panel_LDADD)

EXTRA_DIST = $(SIMPLE_PANEL_NOINST_HEADER_FILES)

CLEANFILES = \
	simple-panel-resources.c \
	simple-panel.gresource.xml
$(top_builddir)/plugins/libbuiltin_plugins.a: $(simple_panel_include_HEADERS)
	cd $(top_builddir)/plugins && $(MAKE) libbuiltin_plugins.a
