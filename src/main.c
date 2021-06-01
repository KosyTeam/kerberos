// Kerberos - Kosy Lock Screen
// Copyright © 2020 Kosy Team <kosyteam.github.io>
// This project is available as GPLv3, see LICENCE.md
#include <glib/gi18n.h>
#include <locale.h>
#include "gtk/gtk.h"
#include "main.h"
#include "actions.h"
#include "wlrp-handler.h"
#include "../config.h"

//declaring important variables
GtkWidget *lockerWindow;
GtkWidget* usernameLabel;
GtkWidget* notifyLabel;
GtkWidget* paswdEntry;
int clearPaswdTimer = 5;
static gboolean isRunning;
GSettings* settings;

//async called function to update the clock and clean password entry of no user action
gboolean update_clock(GtkWidget *clockLabel){
	//get the current time and set it to clock
	GDateTime* now = g_date_time_new_now_local();
	gchar* timeString = g_date_time_format(now, _("%H:%M"));
	
	gtk_label_set_text((GtkLabel*) clockLabel, timeString);
	
	free(timeString);
	g_date_time_unref(now);
	
	
	//delete text of password entry if no action for 5 seconds
	if(clearPaswdTimer <= 0 && strcmp(gtk_editable_get_text((GtkEditable*) paswdEntry), "")){
		gtk_editable_set_text((GtkEditable*) paswdEntry, "");
	} else if (clearPaswdTimer <= 5){
		clearPaswdTimer--;
	}
	
	//return true to continusle call this function
	return(TRUE);
}

//activation function to start lockerWindow
static void activate(GtkApplication *app, gpointer user_data){
	if(isRunning != TRUE){
		isRunning = TRUE;

		//loading ui from xml and connecting signals
		GtkBuilder* builder = gtk_builder_new_from_resource("/eu/kosy/Kerberos/locker.ui");
		lockerWindow = (GtkWidget*) gtk_builder_get_object(builder, "lockerWindow");
		usernameLabel = (GtkWidget*) gtk_builder_get_object(builder, "usernameLabel");
		paswdEntry = (GtkWidget*) gtk_builder_get_object(builder, "paswdEntry");
		GtkWidget* warningLabel = (GtkWidget*) gtk_builder_get_object(builder, "warningLabel");
		GtkWidget* clockLabel = (GtkWidget*) gtk_builder_get_object(builder, "clockLabel");
		notifyLabel = (GtkWidget*) gtk_builder_get_object(builder, "notifyLabel");
		GtkWidget* mPopover = (GtkWidget*) gtk_menu_button_get_popover((GtkMenuButton*) gtk_builder_get_object(builder, "menuButton1"));
		

		//load settings
		settings = g_settings_new("eu.kosy.Kerberos");
		
		//load specific styling
		GtkCssProvider* cssProvider = gtk_css_provider_new();
		GdkDisplay* cssDisplay = gdk_display_get_default();
		
		if(g_settings_get_boolean(settings, "custom-theme") == FALSE){
			gtk_css_provider_load_from_resource(cssProvider, "/eu/kosy/Kerberos/integratedTwo.css");
		} else {
			gtk_css_provider_load_from_path(cssProvider, g_settings_get_string(settings, "custom-theme-path"));
		}
		
		gtk_style_context_add_provider_for_display(cssDisplay,
													(GtkStyleProvider*) cssProvider,
													GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		
		//set background image
		gchar* backgroundUri;
		
		if(g_strcmp0(g_settings_get_string(settings, "background-image"), "") != 0){
			backgroundUri = g_settings_get_string(settings, "background-image");
		} else {
			backgroundUri = "tmpdata/kerberos/default.png";
			if (g_file_test(backgroundUri, G_FILE_TEST_EXISTS) != TRUE){
				backgroundUri = g_build_filename(PGPREFIX, "share/kerberos", "default.png", NULL);
			}
		}
		GtkWidget* backgroundImage = (GtkWidget*) gtk_builder_get_object(builder, "backgroundImage");
		gtk_picture_set_filename((GtkPicture*) backgroundImage, backgroundUri);
		
		//hide the Arrow of GtkMenuButton
		GtkWidget* menuButton1 = (GtkWidget*) gtk_builder_get_object(builder, "menuButton1");
		GtkWidget* menuButtonPopover1 = (GtkWidget*) gtk_menu_button_get_popover((GtkMenuButton*) menuButton1);
		gtk_popover_set_has_arrow((GtkPopover*) menuButtonPopover1, FALSE);
		
		//start clock
		update_clock(clockLabel);
		g_timeout_add_seconds(1, G_SOURCE_FUNC(update_clock), clockLabel);
		
		//set inital text of username label (including name of user)
		char* str = malloc(snprintf(NULL, 0, _("Unlock as <i>%s</i>."), g_get_user_name()));
		sprintf(str, _("Unlock as <i>%s</i>."), g_get_user_name());
		gtk_label_set_markup(GTK_LABEL(usernameLabel), str);
		free(str);
		
		//setting up password entry
		gtk_window_set_default_widget((GtkWindow*) lockerWindow, paswdEntry);
		gtk_widget_grab_focus(paswdEntry);
		gtk_window_set_default_widget((GtkWindow*) lockerWindow, paswdEntry);
		g_signal_connect((GObject*) paswdEntry, "activate", (GCallback) check_passwd, NULL);
		g_signal_connect((GObject*) paswdEntry, "icon-press", (GCallback) check_passwd, NULL);
		g_signal_connect((GObject*) mPopover, "closed", (GCallback) focus_entry, paswdEntry);
		
		//set up caps lock warnung
		GtkEventController* controller = gtk_event_controller_legacy_new();
		gtk_widget_add_controller(paswdEntry, controller);
		g_signal_connect((GObject*) controller, "event", (GCallback) check_shiftlock, warningLabel);
		
		//connect actions
		GSimpleAction* quitAction = g_simple_action_new("quit", NULL);
		g_signal_connect((GObject*) quitAction, "activate", (GCallback) quit_cb, NULL);
		g_action_map_add_action((GActionMap*) app, (GAction*) quitAction);
		
		GSimpleAction* suspendAction = g_simple_action_new("suspend", NULL);
		g_signal_connect((GObject*) suspendAction, "activate", (GCallback) system_call_suspend, paswdEntry);
		g_action_map_add_action((GActionMap*) app, (GAction*) suspendAction);
		
		GSimpleAction* shutdownAction = g_simple_action_new("shutdown", NULL);
		g_signal_connect((GObject*) shutdownAction, "activate", (GCallback) system_call_shutdown, paswdEntry);
		g_action_map_add_action((GActionMap*) app, (GAction*) shutdownAction);
		
		//disalow shutdown if disabled in settings
		if(g_settings_get_boolean(settings, "allow-shutdown") == FALSE){
			g_simple_action_set_enabled(shutdownAction, FALSE);
		}
		
		//connecting to dbus for notifications
		GDBusConnection* dbusConnection = g_application_get_dbus_connection((GApplication*) app);
		GDBusProxy* dbusProxy = g_dbus_proxy_new_sync(dbusConnection, G_DBUS_PROXY_FLAGS_NONE, NULL,
																		"org.freedesktop.Notifications",
																		"/fr/emersion/Mako",
																		"fr.emersion.Mako",
																		NULL, NULL);
		if(check_notifications(dbusProxy) == TRUE){
			g_timeout_add_seconds(5, G_SOURCE_FUNC(check_notifications), dbusProxy);
		} else {
			g_warning("No connection to Mako established.\n");
			gtk_widget_hide(notifyLabel);
		}
			
		//present the window
		gtk_window_set_application(GTK_WINDOW(lockerWindow), app);
		gtk_window_present((GtkWindow*) lockerWindow);
		
		gtk_window_fullscreen((GtkWindow*) lockerWindow); //TODO should use layer-shell (as soon as Gtk4 is supported)
		
		//call the input inhibiter
		inhibit_input();
	}
}

// the main function calling the application itself
int main(int argc, char **argv){
	GtkApplication *app;
	int status;
	
	//seting up gettext
	setlocale(LC_ALL, "");
	bindtextdomain(GETTEXT_PACKAGE, PGDATA "/locale");
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);

	//start app window
	app = gtk_application_new("eu.kosy.Kerberos", G_APPLICATION_FLAGS_NONE);
	g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
	status = g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);

	return(status);
}
