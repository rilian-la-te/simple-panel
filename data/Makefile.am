default_config_DATA = \
	default/config
default_configdir = $(sysconfdir)/xdg/simple-panel/default

default_config_panel_DATA = \
	default/panels/panel
default_config_paneldir = $(sysconfdir)/xdg/simple-panel/default/panels

two_panels_config_DATA = \
	two_panels/config
two_panels_configdir = $(sysconfdir)/xdg/simple-panel/two_panels

two_panels_config_panel_DATA = \
	two_panels/panels/bottom \
	two_panels/panels/top
two_panels_config_paneldir = $(sysconfdir)/xdg/simple-panel/two_panels/panels

@INTLTOOL_XML_NOMERGE_RULE@

gsettings_ENUM_NAMESPACE = org.simple.panel
gsettings_ENUM_FILES = \
        $(top_srcdir)/src/*.h \
        $(top_srcdir)/plugins/*.c \
        $(top_srcdir)/plugins/X/*.c \
        $(top_srcdir)/plugins/xkb/xkb.h

panel_gschemas_in_in = \
        gschemas/org.simple.panel.gschema.xml.in.in \
        gschemas/org.simple.panel.toplevel.gschema.xml.in.in \
        gschemas/org.simple.panel.toplevel.plugin.gschema.xml.in.in \
        gschemas/org.simple.panel.builtin.gschema.xml.in.in \
        gschemas/org.simple.panel.X.gschema.xml.in.in \
        gschemas/org.simple.panel.plugins.gschema.xml.in.in

panel_gschemas_in = $(panel_gschemas_in_in:.xml.in.in=.xml.in)
gsettings_SCHEMAS = $(panel_gschemas_in:.xml.in=.xml)

%.gschema.xml.in: %.gschema.xml.in.in Makefile
	$(AM_V_GEN) $(SED) -e 's^\@GETTEXT_PACKAGE\@^$(GETTEXT_PACKAGE)^g' < $< > $@

@GSETTINGS_RULES@

uidir=$(datadir)/simple-panel/ui
ui_in_files= \
	ui/launchtaskbar.glade \
	ui/netstatus.glade

ui_DATA = $(ui_in_files:.glade=.ui)

# Purge GtkBuilder UI files
.glade.ui:
	sed 's/<!--.*-->//' < $< | sed ':a;N;$$!ba;s/ *\n *</</g' > $@

CLEANFILES = \
	$(ui_DATA) \
	$(panel_gschemas_in) \
	$(gsettings_SCHEMAS)

simple_panel_images_DATA = \
	images/my-computer.png \
	images/file-manager.png \
	images/gnome-fs-desktop.png \
	images/gnome-netstatus-0-24.png \
	images/gnome-netstatus-25-49.png \
	images/gnome-netstatus-50-74.png \
	images/gnome-netstatus-75-100.png \
	images/gnome-netstatus-disconn.png \
	images/gnome-netstatus-error.png \
	images/gnome-netstatus-idle.png \
	images/gnome-netstatus-rx.png \
	images/gnome-netstatus-txrx.png \
	images/gnome-netstatus-tx.png \
	images/background.png \
	images/ns-bothrs.png \
	images/ns-connected.png \
	images/ns-disconnect.png \
	images/ns-problem.png \
	images/ns-recvdata.png \
	images/ns-senddata.png \
	images/ns-lock.png \
	images/capslock-on.png \
	images/capslock-off.png \
	images/numlock-on.png \
	images/numlock-off.png \
	images/scrllock-on.png \
	images/scrllock-off.png \
	images/window-manager.png \
	images/clock.png \
	images/cpufreq-icon.png


simple_panel_imagesdir = $(datadir)/simple-panel/images

EXTRA_DIST = \
	default/panels/panel.in \
	$(simple_panel_DATA) \
	$(simple_panel_images_DATA) \
	$(ui_DATA) \
	$(panel_gschemas_in_in) \
	$(ui_in_files) \
	$(default_config_DATA) \
	$(two_panels_config_DATA)
