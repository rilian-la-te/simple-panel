using GLib;
using Gtk;
using Config;
using PanelCSS;

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
	internal static string system_config_file_name(string name1, string profile,
											string? name2)
	{
		return GLib.Path.build_filename(name1,GETTEXT_PACKAGE,profile,name2);
	}
	internal static string user_config_file_name(string name1, string profile,
											string? name2)
	{
		return GLib.Path.build_filename(GLib.Environment.get_user_config_dir(),
								GETTEXT_PACKAGE,profile,name1,name2);
	}
	public static int main(string[] args)
	{
		var app = new App();
		return app.run();
	}
	
	public class App: Gtk.Application
	{
		private static const string SCHEMA = "org.simple.panel";
		private static const string NAME = "global";
		private static const string PATH = "/org/simple/panel/";
		private bool started;
		private SettingsBackend config_backend;
		private Dialog? pref_dialog;
		private GLib.Settings config;
		private bool _dark;
		private bool _custom;
		private string _css;
		private CssProvider provider;
		public string profile
		{get; private set;}
		public string terminal_command
		{get; private set;}
		public string logout_command
		{get; private set;}
		public string shutdown_command
		{get; private set;}
		public bool is_dark
		{get {return _dark;}
		private set {_dark = value; apply_styling();}}
		public bool is_custom
		{get {return _custom;}
		private set {_custom = value; apply_styling();}}
		public string css
		{get {return _css;}
		private set {_css = value; apply_styling();}}
				
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
			{"panel-preferences", activate_panel_preferences_callback, "s", null, null},
			{"about", activate_about, null, null, null},
			{"menu", activate_menu_callback, null, null, null},
			{"run", activate_run, null, null, null},
			{"logout", activate_logout, null, null, null},
			{"shutdown", activate_shutdown, null, null, null},
			{"quit", activate_exit, null, null, null},
			{ null }
		};
		private static const GLib.ActionEntry[] menu_entries =
		{
			{"launch-id", activate_menu_launch_id, "s", null, null},
			{"launch-uri", activate_menu_launch_uri, "s", null, null},
			{"launch-command", activate_menu_launch_command, "s", null, null},
		};
		public App()
		{
			Object(application_id: "org.valapanel.application",
					flags: GLib.ApplicationFlags.HANDLES_COMMAND_LINE);
		}
		
		construct
		{
			started = false;
			add_main_option_entries(options);
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
			pref_dialog.run();
			pref_dialog.destroy();
		}
		
		public override void startup()
		{
			base.startup();
			GLib.Intl.setlocale(LocaleCategory.CTYPE,"");
			GLib.Intl.bindtextdomain(Config.GETTEXT_PACKAGE,Config.PACKAGE_LOCALE_DIR);
			GLib.Intl.bind_textdomain_codeset(Config.GETTEXT_PACKAGE,"UTF-8");
			GLib.Intl.textdomain(Config.GETTEXT_PACKAGE);
			Gtk.IconTheme.get_default().append_search_path(Config.PACKAGE_DATA_DIR+"/images");
			Compat.fm_gtk_init();
			Compat.prepare_modules();
			add_action_entries(app_entries,this);
			add_action_entries(menu_entries,this);
		}
		public override void shutdown()
		{
			Compat.unload_modules();
			Compat.fm_gtk_finalize();
			base.shutdown();
		}
		
		public override void activate()
		{
			if (!started)
			{
				ensure_user_config();
				load_settings();
				Gdk.get_default_root_window().set_events(Gdk.EventMask.STRUCTURE_MASK
														|Gdk.EventMask.SUBSTRUCTURE_MASK
														|Gdk.EventMask.PROPERTY_CHANGE_MASK);
				if (!start_all_panels())
				{
					warning("Config files are not found\n");
					this.quit();
				}
				else
				{
					started = true;
					apply_styling();
				}
			}
		}
		
		public override int handle_local_options(VariantDict opts)
		{
			if (opts.contains("version"))
			{
				stdout.printf(_("%s - Version %s\n"),GLib.Environment.get_application_name(),
													Config.VERSION);
				return 0;
			}
			return -1;
		}
		public override int command_line(ApplicationCommandLine cmdl)
		{
			string? profile_name;
			string? command;
			var options = cmdl.get_options_dict();
			if (options.lookup("profile","s",out profile_name))
				profile = profile_name;
			if (options.lookup("command","s",out command))
			{
				string name;
				Variant param;
				try
				{
					GLib.Action.parse_detailed_name(command,out name,out param);
				}
				catch (Error e)
				{
					cmdl.printerr("%s: invalid command. Cannot parse.", command);
				}
				var action = lookup_action(name);
				if (action != null)
					action.activate(param);
				else
				{
					var actions = string.joinv(" ",list_actions());
					cmdl.printerr(_("%s: invalid command - %s. Doing nothing.\nValid commands: %s\n"),
									GLib.Environment.get_application_name(),actions);
				}
			}
			activate();
			return 0;
		}
		
		private bool start_all_panels()
		{
			var panel_dir = user_config_file_name("panels",profile,null);
			Compat.start_panels_from_dir((Gtk.Application)this,panel_dir);
			if (this.get_windows() != null)
				return true;
			var dirs = GLib.Environment.get_system_config_dirs();
			foreach(var dir in dirs)
			{
				panel_dir = system_config_file_name(dir,profile,"panels");
				if (this.get_windows() != null)
					return true;
			}
			return (this.get_windows() != null);
		}
		
		private void ensure_user_config()
		{
			var dir = user_config_file_name("panels",profile,null);
			GLib.FileUtils.mkdir_with_parents(dir,0700);
		}
		
		private void apply_styling()
		{
			if (Gtk.Settings.get_default()!=null)
				Gtk.Settings.get_default().set("gtk-application-prefer-dark-theme",is_dark,null);
			if (is_custom)
			{
				if (provider!=null)
					Gtk.StyleContext.remove_provider_for_screen(Gdk.Screen.get_default(),provider);
				PanelCSS.apply_from_file_to_app_with_provider(css);
			}
		}
		private void load_settings()
		{
			var dirs = GLib.Environment.get_system_config_dirs();
			var loaded = false;
			string? file = null;
			string? user_file = null;
			foreach (var dir in dirs)
			{
				file = system_config_file_name(dir,profile,"config");
				if (GLib.FileUtils.test(file,FileTest.EXISTS))
				{
					loaded = true;
					break;
				}
			}
			user_file = user_config_file_name("config",profile,null);
			if (!GLib.FileUtils.test(user_file,FileTest.EXISTS) && loaded)
			{
				var src = File.new_for_path(file);
				var dest = File.new_for_path(user_file);
				try{src.copy(dest,FileCopyFlags.BACKUP,null,null);}
				catch(Error e){warning("Cannot init global config: %s\n",e.message);}
			}
			config_backend = GLib.keyfile_settings_backend_new(user_file,PATH,NAME);
			config = new GLib.Settings.with_backend_and_path(SCHEMA,config_backend,PATH);
			
		}
		private void activate_menu_callback(SimpleAction action, Variant? param)
		{
			Compat.activate_menu(action,param,this);
		}
		private void activate_panel_preferences_callback(SimpleAction action, Variant? param)
		{
			Compat.activate_panel_preferences(action,param,this);
		}
		internal void activate_menu_launch_id(SimpleAction action, Variant? param)
		{
			var id = param.get_string();
			var info = new DesktopAppInfo(id);
			try{
			info.launch(null,Gdk.Display.get_default().get_app_launch_context());
			} catch (GLib.Error e){warning("%s\n",e.message);}
		}
		internal void activate_menu_launch_command(SimpleAction action, Variant? param)
		{
			var command = param.get_string();
			GLib.AppInfo info = AppInfo.create_from_commandline(command,null,
						AppInfoCreateFlags.SUPPORTS_STARTUP_NOTIFICATION);
			try{
			info.launch(null,Gdk.Display.get_default().get_app_launch_context());
			} catch (GLib.Error e){warning("%s\n",e.message);}
		}
		internal void activate_menu_launch_uri(SimpleAction action, Variant? param)
		{
			var uri = param.get_string();
			try{
			GLib.AppInfo.launch_default_for_uri(uri,
						Gdk.Display.get_default().get_app_launch_context());
			} catch (GLib.Error e){warning("%s\n",e.message);}
		}
		internal void activate_about(SimpleAction action, Variant? param)
		{
			
		}
		internal void activate_run(SimpleAction action, Variant? param)
		{
			
		}
		internal void activate_logout(SimpleAction action, Variant? param)
		{
			
		}
		internal void activate_shutdown(SimpleAction action, Variant? param)
		{
			
		}
		internal void activate_exit(SimpleAction action, Variant? param)
		{
			this.quit();
		}
	}
}
