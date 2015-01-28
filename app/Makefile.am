## Process this file with automake to produce Makefile.in

bin_PROGRAMS =simple-panel

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

vala_panel_resdeps = $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies vala-panel.gresource.xml)

vala-panel-resources.c: vala-panel.gresource.xml $(simple_panel_resdeps)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source vala-panel.gresource.xml

BUILT_SOURCES = \
	vala-panel-resources.c

nodist_simple_panel_SOURCES = vala-panel-resources.c
simple_panel_SOURCES = runner.vala application.vala

simple_panel_VALAFLAGS = \
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

simple_panel_LDADD = \
		$(top_builddir)/lib/libsimplepanel.la \
		$(top_builddir)/libvalapanel/libvalapanel.la \
		$(PACKAGE_LIBS) \
		$(INTLLIBS)
##		$(X11_LIBS) \

EXTRA_DIST = $(SIMPLE_PANEL_NOINST_HEADER_FILES)

CLEANFILES = \
	vala-panel-resources.c \
	vala-panel.gresource.xml \
	$(simple_panel_SOURCES:.vala=.c)
