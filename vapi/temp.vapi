[CCode (prefix = "", lower_case_cprefix = "", cheader_filename = "../lib/misc.h")]
namespace Compat 
{
	namespace Key 
	{
		[CCode (cname = "PANEL_PROP_ICON_SIZE", cheader_filename = "../lib/panel.h")]
		public static string ICON_SIZE;
	}
	[CCode (cname = "activate_panel_preferences", cheader_filename = "../lib/misc.h")]
	public static void activate_panel_preferences(GLib.SimpleAction action, GLib.Variant? param, void* data);
	[CCode (cname = "activate_menu", cheader_filename = "../lib/misc.h")]
	public static void activate_menu(GLib.SimpleAction action, GLib.Variant? param, void* data);
	[CCode (cname = "_prepare_modules", cheader_filename = "../lib/private.h")]
	public static void prepare_modules();
	[CCode (cname = "_unload_modules", cheader_filename = "../lib/private.h")]
	public static void unload_modules();
	[CCode (cname = "init_pugin_class_list", cheader_filename = "../lib/private.h")]
	public static void init_plugin_class_list();
	[CCode (cname = "fm_gtk_init", cheader_filename = "libfm/fm-gtk.h")]
	public static void fm_gtk_init(GLib.Object? o);
	[CCode (cname = "fm_gtk_finalize", cheader_filename = "libfm/fm-gtk.h")]
	public static void fm_gtk_finalize();
	[CCode (cname = "SimplePanel", cheader_filename = "../lib/panel.h")]
	public class Toplevel : Gtk.ApplicationWindow
	{
		[NoAccessorMethod]
		public int icon_size
		{get; set;}
		[CCode (cname="panel_load", cheader_filename = "../lib/private.h", returns_floating_reference=true)]
		public static Toplevel? load(Gtk.Application app, string config_file, string config_name);
	}
}
