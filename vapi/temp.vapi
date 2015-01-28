[CCode (prefix = "", lower_case_cprefix = "", cheader_filename = "misc.h")]
namespace Compat 
{
	[CCode (cname = "start_panels_from_dir", cheader_filename = "misc.h")]
	public static void start_panels_from_dir(Gtk.Application app, string dir);
	[CCode (cname = "activate_panel_preferences", cheader_filename = "misc.h")]
	public static void activate_panel_preferences(GLib.SimpleAction action, GLib.Variant? param, void* data);
	[CCode (cname = "activate_menu", cheader_filename = "misc.h")]
	public static void activate_menu(GLib.SimpleAction action, GLib.Variant? param, void* data);
	[CCode (cname = "_prepare_modules", cheader_filename = "private.h")]
	public static void prepare_modules();
	[CCode (cname = "_unload_modules", cheader_filename = "private.h")]
	public static void unload_modules();
	[CCode (cname = "init_pugin_class_list", cheader_filename = "private.h")]
	public static void init_plugin_class_list();
	[CCode (cname = "fm_gtk_init", cheader_filename = "libfm/fm-gtk.h")]
	public static void fm_gtk_init(GLib.Object? o);
	[CCode (cname = "fm_gtk_finalize", cheader_filename = "libfm/fm-gtk.h")]
	public static void fm_gtk_finalize();
}
