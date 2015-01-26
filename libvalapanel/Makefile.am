EXTRA_DIST = 

panel_resources = $(shell glib-compile-resources --sourcedir=$(top_srcdir)/data --generate-dependencies $(top_srcdir)/data/libvalapanel.gresource.xml)

BUILT_SOURCES = \
	libvalapanel-resources.h \
	libvalapanel-resources.c

# resources
libvalapanel-resources.h: $(top_srcdir)/data/libvalapanel.gresource.xml
	$(AM_V_GEN) glib-compile-resources --target=$@ --sourcedir=$(top_srcdir)/data --generate-header $(top_srcdir)/data/libvalapanel.gresource.xml

libvalapanel-resources.c: $(top_srcdir)/data/libvalapanel.gresource.xml $(panel_resources)
	$(AM_V_GEN) $(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(top_srcdir)/data --generate-source $(top_srcdir)/data/libvalapanel.gresource.xml


pkglib_LTLIBRARIES = libvalapanel.la

libvalapanel_la_CPPFLAGS = $(simple_panel_CPPFLAGS)
nodist_libvalapanel_la_SOURCES = libvalapanel-resources.c
libvalapanel_la_SOURCES = \
	menu-maker.vala \
	plugin-settings.vala \
	icon-grid.vala \
	css.vala

libvalapanel_la_VALAFLAGS = \
	--library=libvalapanel \
	-H vala.h \
	--vapi=$(top_srcdir)/vapi/vala-panel.vapi \
	--vapidir=$(top_srcdir)/vapi \
	--vapidir=. \
	--pkg gtk+-3.0 \
	--pkg gio-2.0 \
	--pkg gio-unix-2.0 \
	--pkg gio-settingsbackend-2.0 \
	$(VALAFLAGS)

libvalapanel_la_CFLAGS = \
	$(PACKAGE_CFLAGS) \
	-DG_SETTINGS_ENABLE_BACKEND \
	-DGETTEXT_PACKAGE=\""simple-panel"\" \
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)/simple-panel"\" \
	-DPACKAGE_UI_DIR=\""$(datadir)/simple-panel/ui"\" \
	-DPACKAGE_BIN_DIR=\""$(bindir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

libvalapanel_la_LIBADD = \
	$(PACKAGE_LIBS)
	
libvalapanel_la_LDFLAGS = \
       -no-undefined \
       -rpath $(pkglibdir)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(libvalapanel_la_SOURCES:.vala=.c) \
	vala.h

dist-hook:
	cd $(distdir) && \
	rm $(libvalapanel_la_SOURCES:.vala=.c) libvalapanel_la_vala.stamp
