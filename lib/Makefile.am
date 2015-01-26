## Process this file with automake to produce Makefile.in

simple_panel_CPPFLAGS = \
	-I$(top_srcdir) \
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)/simple-panel"\" \
	-DPACKAGE_UI_DIR=\""$(datadir)/simple-panel/ui"\" \
	-DPACKAGE_BIN_DIR=\""$(bindir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	$(PACKAGE_CFLAGS) \
	$(G_CAST_CHECKS)

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
	private.h \
	menu-maker.h

SIMPLE_PANEL_ENUM_FILES = $(SIMPLE_PANEL_HEADER_FILES)
SIMPLE_PANEL_ENUM_FILES += $(SIMPLE_PANEL_NOINST_HEADER_FILES)

libsimplpanel_la_resdeps = $(shell $(GLIB_COMPILE_RESOURCES) --generate-dependencies libsimplepanel.gresource.xml)

libsimplepanel-resources.c: libsimplepanel.gresource.xml $(libsimplpanel_la_resdeps)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate-source libsimplepanel.gresource.xml

panel-enum-types.h: panel-enum-types.h.template $(SIMPLE_PANEL_ENUM_FILES) $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template panel-enum-types.h.template $(SIMPLE_PANEL_ENUM_FILES)) > $@

panel-enum-types.c: panel-enum-types.c.template $(SIMPLE_PANEL_ENUM_FILES) $(GLIB_MKENUMS)
	$(AM_V_GEN) (cd $(srcdir) && $(GLIB_MKENUMS) --template panel-enum-types.c.template $(SIMPLE_PANEL_ENUM_FILES)) > $@

BUILT_SOURCES = \
	panel-enum-types.c \
	panel-enum-types.h \
	libsimplepanel-resources.c \
	vala.h

libsimplepanel_la_CPPFLAGS = $(simple_panel_CPPFLAGS)
nodist_libsimplepanel_la_SOURCES = libsimplepanel-resources.c
libsimplepanel_la_SOURCES = \
	misc.c \
	css.c \
	menu-maker.c \
	configurator.c \
	dbg.c \
	icon-grid.c \
	panel.c \
	plugin.c \
	conf-gsettings.c \
	panel-enum-types.c
	
libsimplepanel_la_LDFLAGS = \
       -no-undefined \
       -rpath $(pkglibdir)
       
libsimplepanel_la_LIBADD = \
	$(PACKAGE_LIBS)

EXTRA_DIST = $(SIMPLE_PANEL_NOINST_HEADER_FILES) \
	panel-enum-types.h

CLEANFILES = panel-enum-types.c \
	panel-enum-types.h \
	libsimplepanel-resources.c \
	libsimplepanel.gresource.xml 
