// Kerberos - Kosy Lockscreen
// Copyright © 2020 Kosy Team <kosyteam.github.io>
// This project is available as GPLv3, see LICENCE.md
#include <glib/gi18n.h>
#include <locale.h>
#include "gtk/gtk.h"
#include "main.h"
#include "actions.h"
#include <security/pam_appl.h>
#include <security/pam_misc.h>

//declaring important variables
struct pam_response *reply;

//system call to generic script
void system_call_generic(){
	system("");
}

//system call to suspend
void system_call_suspend(GtkWidget* paswdEntry){
	system("systemctl suspend");
	gtk_widget_grab_focus(paswdEntry);
}

//system call to shutdown
void system_call_shutdown(GtkWidget* paswdEntry){
	system("systemctl poweroff");
	gtk_widget_grab_focus(paswdEntry);
}

//quit function to stop application
void quit_cb(GObject *obj){
	gtk_window_set_application((GtkWindow*) lockerWindow, NULL);
	gtk_window_close((GtkWindow*) lockerWindow);
}

//check for shitftlock
void check_shiftlock(GtkEventControllerKey* controller, GdkEvent* event, GtkWidget* warningLabel){
	//get keyboard
	GdkDevice* keyboard  = (gdk_seat_get_keyboard(gdk_display_get_default_seat(gdk_display_get_default())));
	
	//show or hide the label debending on state
	if (gdk_device_get_caps_lock_state(keyboard)){
		gtk_widget_show(warningLabel);
	} else {
		gtk_widget_hide(warningLabel);
	}
	
	//this has nothing to do with shiftlock it's for reseting the timer before the entries text gets deleted
	clearPaswdTimer = 5;
}

//function to check for notifications
gboolean check_notifications(GDBusProxy* dbusProxy){
	GVariant* callVariant = g_dbus_proxy_call_sync(dbusProxy, "ListNotifications", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL);
	
	//check if mako is providing the notification daemon, if not return false
	if (callVariant == NULL){
		return(FALSE);
	}
	
	GVariantIter *iter;
	g_variant_get(callVariant, "(aa{sv})", &iter);
	int counter = 0;
	while(g_variant_iter_loop(iter, "a{sv}", NULL)){
		counter++;
	}
	g_variant_unref(callVariant);
	
	set_notifications(notifyLabel, counter);
	return(TRUE);
}
	

//function settings notification number in label
void set_notifications(GtkWidget* notifyLabel, int counter){
	switch(counter){
		case 0:
			gtk_label_set_text(GTK_LABEL(notifyLabel), _("No new notifications."));
			break;
		case 1:
			gtk_label_set_text(GTK_LABEL(notifyLabel), _("One new notifications."));
			break;
		default:
			char* str2 = malloc(snprintf(NULL, 0, _("%d new notifications."), counter));
			sprintf(str2, _("%d new notifications."), counter);
			gtk_label_set_text(GTK_LABEL(notifyLabel), str2);
			g_free(str2);
			break;
	}
}

//pam conversation function
int function_conversation(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr)  
{  
    *resp = reply;
    return PAM_SUCCESS;  
}

//this function gets called to check password and returns 0 if true
static int check_passwd_backend(const char *username, const char *password){
	//set vars
	const struct pam_conv local_conversation = { function_conversation, NULL };
	pam_handle_t *local_auth_handle = NULL;
    int retval;
    int result;
    
    //start pam conversation
    retval = pam_start("su", username, &local_conversation, &local_auth_handle);
    
    //set password in pam reply and authentificate
    reply = (struct pam_response *)malloc(sizeof(struct pam_response));
    reply[0].resp = strdup(password);
    reply[0].resp_retcode = 0;
    retval = pam_authenticate(local_auth_handle, 0);
	
	//check answer
	if(retval != PAM_SUCCESS){
		result = 1;
	} else {
		result = 0;
	}
	
	//end pam and return
	pam_end(local_auth_handle, 0);
	return(result);
}

//function to check the password
void check_passwd(GtkWidget* passwdEntry){
	//update label
	gtk_label_set_text(GTK_LABEL(usernameLabel), _("Verifing input"));
	while (! g_main_context_iteration(NULL, TRUE));
	while (! g_main_context_iteration(NULL, FALSE));
	
	//check the password
	int paswdResult = check_passwd_backend(g_get_user_name(), gtk_editable_get_text((GtkEditable*) passwdEntry));
	if (paswdResult == 0){
		//update label
		gtk_label_set_text(GTK_LABEL(usernameLabel), _("Unlocking system"));
		while (! g_main_context_iteration(NULL, TRUE));
		while (! g_main_context_iteration(NULL, FALSE));
		//quit lockscreen
		quit_cb(NULL);
	} else {
		//update label and clear entry
		gtk_label_set_text(GTK_LABEL(usernameLabel), _("Wrong input, please try again."));
		gtk_editable_set_text((GtkEditable*) passwdEntry, "");
	}
}

//function to refocus PasswordEntry (e.g. after menu popup close)
void focus_entry(GtkWidget* widget, GtkWidget* paswdEntry){
	gtk_widget_grab_focus(paswdEntry);
}
