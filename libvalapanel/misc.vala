using GLib;
using Gtk;

namespace ValaPanel
{
	public static void activate_menu_launch_id(SimpleAction action, Variant? param)
	{
		var id = param.get_string();
		var info = new DesktopAppInfo(id);
		try{
		info.launch(null,Gdk.Display.get_default().get_app_launch_context());
		} catch (GLib.Error e){stderr.printf("%s\n",e.message);}
	}
	public static void activate_menu_launch_command(SimpleAction? action, Variant? param)
	{
		var command = param.get_string();
		try{
		GLib.AppInfo info = AppInfo.create_from_commandline(command,null,
					AppInfoCreateFlags.SUPPORTS_STARTUP_NOTIFICATION);
		info.launch(null,Gdk.Display.get_default().get_app_launch_context());
		} catch (GLib.Error e){stderr.printf("%s\n",e.message);}
	}
	public static void activate_menu_launch_uri(SimpleAction action, Variant? param)
	{
		var uri = param.get_string();
		try{
		GLib.AppInfo.launch_default_for_uri(uri,
					Gdk.Display.get_default().get_app_launch_context());
		} catch (GLib.Error e){stderr.printf("%s\n",e.message);}
	}
	public static void gsettings_as_action(ActionMap map, GLib.Settings settings, string prop)
	{
		settings.bind(prop,map,prop,SettingsBindFlags.GET|SettingsBindFlags.SET|SettingsBindFlags.DEFAULT);
		var action = settings.create_action(prop);
		map.add_action(action);
	}
	public static int apply_menu_properties(List<Widget> widgets, MenuModel menu)
	{
		var len = menu.get_n_items();
		unowned List<Widget> l = widgets;
		var i = 0;
		for(i = 0, l = widgets;(i<len)&&(l!=null);i++,l=l.next)
		{
			while (l.data is SeparatorMenuItem) l = l.next;
			var shell = l.data as Gtk.MenuItem;
			var menuw = shell.get_submenu() as Gtk.Menu;
			var menu_link = menu.get_item_link(i,"submenu");
			if (menuw !=null && menu_link !=null)
				apply_menu_properties(menuw.get_children(),menu_link);
			menu_link = menu.get_item_link(i,"section");
			if (menu_link != null)
			{
				var ret = apply_menu_properties(l,menu_link);
				for (int j=0;j<ret;j++) l = l.next;
			}
			string? str = null;
			menu.get_item_attribute(i,"icon","s",out str);
			if(str != null)
			{
				try
				{
					var icon = Icon.new_for_string(str);
					shell.set("icon",icon);
				} catch (Error e)
				{
					stderr.printf("Incorrect menu icon:%s\n", e.message);
				}
			}
			str=null;
			menu.get_item_attribute(i,"tooltip","s",out str);
			if (str != null)
				shell.set_tooltip_text(str);
		}
		return i-1;
	}
	
	public static void setup_button(Widget button, Widget? image, string? label)
	{
		Button b = button as Button;
		Image img = image as Image;
		PanelCSS.apply_from_resource(b,"/org/vala-panel/lib/style.css","-panel-button");
//Children hierarhy: button => alignment => box => (label,image)
		b.notify.connect((a,b)=>{
			if (b.name == "label" || b.name == "image")
			{
				var B = a as Bin;
				var w = B.get_child();
				if (w is Container)
				{
					Bin? bin;
					Widget ch;
					if (w is Bin)
					{
						bin = w as Bin;
						ch = bin.get_child();
					}
					else
						ch = w;
					if (ch is Container)
					{
						var cont = ch as Container;
						cont.forall((c)=>{
							if (c is Widget){
							c.set_halign(Gtk.Align.FILL);
							c.set_valign(Gtk.Align.FILL);
							}});
					}
					ch.set_halign(Gtk.Align.FILL);
					ch.set_valign(Gtk.Align.FILL);
				}
			}
		});
		if (img != null)
		{
			b.set_image(img);
			b.set_always_show_image(true);
		}
		if (label != null)
			b.set_label(label);
		b.set_relief(Gtk.ReliefStyle.NONE);
	}
}
