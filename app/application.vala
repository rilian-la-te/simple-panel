using GLib;
using Gtk;
using Config;

namespace ValaPanel
{
	namespace Key
	{
		public static const string LOGOUT = "logout-command";
		public static const string SHUTDOWN = "shutdown-command";
		public static const string TERMINAL = "terminal-command";
		public static const string DARK = "is-dark";
		public static const string CUSTOM = "is-custom";
		public static const string CSS = "css";
	}
	public static int main(string[] args)
	{
		var app = new App();
		return app.run();
	}
	
	public class App: Gtk.Application
	{
		private bool started;
		private Dialog? pref_dialog;
		private GLib.Settings config;
		public string css
		{get; private set;}
		
		private static const OptionEntry options[] =
		{
			{ "version", 'v', 0, OptionArg.NONE, null, N_("Print version and exit"), null },
			{ "profile", 'p', 0, OptionArg.STRING, null, N_("Use specified profile"), N_("profile") },
			{ "command", 'c', 0, OptionArg.STRING, null, N_("Run command on already opened panel"), N_("cmd") },
			{ null }
		};
		private static const GLib.ActionEntry[] app_entries =
		{
			{"preferences", activate_preferences, null, null, null},
//			{"panel-preferences", Compat.activate_panel_preferences, "s", null, null},
//			{"about", activate_about, null, null, null},
//			{"menu", Compat.activate_menu, null, null, null},
//			{"run", activate_run, null, null, null},
//			{"logout", activate_logout, null, null, null},
//			{"shutdown", activate_shutdown, null, null, null},
//			{"quit", activate_exit, null, null, null},
			{ null }
		};
//		private static const ActionEntry menu_entries =
//		{
//			{"launch-id", activate_menu_launch_id, "s", null, null},
//			{"launch-uri", activate_menu_launch_uri, "s", null, null},
//			{"launch-command", activate_menu_launch_command, "s", null, null},
//		};
		public App()
		{
			Object(application_id: "org.valapanel.application",
					flags: GLib.ApplicationFlags.HANDLES_COMMAND_LINE);
		}
		
		private void activate_preferences (SimpleAction act, Variant? param)
		{
			if(pref_dialog!=null)
			{
				pref_dialog.present();
				return;
			}
			var builder = new Builder.from_resource("/org/vala-panel/app/pref.ui");
			pref_dialog = builder.get_object("app-pref") as Dialog;
			this.add_window(pref_dialog);
			var w = builder.get_object("logout") as Widget;
			config.bind(Key.LOGOUT,w,"text",SettingsBindFlags.DEFAULT);
			w = builder.get_object("shutdown") as Widget;
			config.bind(Key.SHUTDOWN,w,"text",SettingsBindFlags.DEFAULT);
			w = builder.get_object("css-chooser") as Widget;
			config.bind(Key.CUSTOM,w,"sensitive",SettingsBindFlags.DEFAULT);
			var f = w as FileChooserButton;
			f.set_filename(css);
			f.file_set.connect((a)=>{
				var file = f.get_filename();
				if (file != null)
				{
					this.activate_action(Key.CSS,file);
				}
			});
			var response = pref_dialog.run();
		}
		
		public override void startup()
		{
			GLib.Intl.setlocale(LocaleCategory.CTYPE,"");
			GLib.Intl.bindtextdomain(Config.GETTEXT_PACKAGE,Config.PACKAGE_LOCALE_DIR);
			GLib.Intl.bind_textdomain_codeset(Config.GETTEXT_PACKAGE,"UTF-8");
			GLib.Intl.textdomain(Config.GETTEXT_PACKAGE);
			Gtk.IconTheme.get_default().append_search_path(Config.PACKAGE_DATA_DIR+"/images");
		}
	}
}
