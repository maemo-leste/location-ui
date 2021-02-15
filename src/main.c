/*
 * Copyright (c) 2021 Ivan J. <parazyd@dyne.org>
 *
 * This file is part of location-ui
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <locale.h>
#include <libintl.h>
#include <stdlib.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib.h>
#include <gtk/gtk.h>

#include <hildon/hildon.h>

/* enums */
#define LUI_DBUS_NAME    "com.nokia.Location.UI"
#define LUI_DBUS_DIALOG  LUI_DBUS_NAME".Dialog"
#define LUI_DBUS_PATH    "/com/nokia/location/ui"

enum {
	STATE_0,
	STATE_QUEUE,
	STATE_2,
};

typedef struct dialog_data_t {
	GtkWindow *window;
	char *dbus_object_path;
	char *foo2;
	char *path;
	char *maybe_path;
	int dialog_response_code;
	int dialog_active;
} dialog_data_t;

typedef struct location_ui_dialog {
	char *maybe_dialog_instance;
	const char *path;
	int (*get_instance)(void);
	int maybe_priority;
	int int_ffffffff_everywhere;
	int state;
} location_ui_dialog;

typedef struct location_ui_t {
	GList *dialogs;
	gpointer current_dialog;
	DBusConnection *dbus;
	guint inactivity_timeout_id;
} location_ui_t;

/* function declarations */
static GtkWidget *create_disclaimer_dialog(void);
static GtkWidget *create_positioning_dialog(void);
static GtkWidget *create_enable_gps_dialog(void);
static GtkWidget *create_enable_network_dialog(void);
static GtkWidget *create_agnss_dialog(void);
static GtkWidget *create_bt_disconnect_dialog(void);
static GtkWidget *create_privacy_verification_dialog(DBusMessage *,
						     DBusError *);
static GtkWidget *create_privacy_information_dialog(DBusMessage *, DBusError *);
static GtkWidget *create_privacy_timeout_dialog(DBusMessage *, DBusError *);
static GtkWidget *create_privacy_expired_dialog(DBusMessage *, DBusError *);
static GtkWidget *create_default_supl_dialog(DBusMessage *, DBusError *);
static GtkWidget *create_bt_disabled_dialog(void);
static DBusMessage *location_ui_close_dialog(location_ui_t *, GList *,
					     DBusMessage *);
static DBusMessage *location_ui_display_dialog(location_ui_t *, GList *,
					       DBusMessage *);
static int compare_dialog_path(location_ui_t *, const char *);
static int find_dbus_cb(int, DBusMessage *, location_ui_t *);
static int on_inactivity_timeout(location_ui_t *);
static void on_dialog_response(GtkWidget *, int, location_ui_t *);
static location_ui_dialog *find_next_dialog(location_ui_t *);
static void schedule_new_dialog(location_ui_t *);

/* variables */
static DBusObjectPathVTable vtable;
static DBusObjectPathVTable find_callback_vtable;
static struct dialog_data_t funcmap[7];
static DBusMessage *(*display_close_map[2])() =
    { location_ui_close_dialog, location_ui_display_dialog };

GtkWidget *create_disclaimer_dialog(void)
{
	char *disclaimer_text, *disclaimer_ok, *disclaimer_reject;
	char *fi_disclaimer_text;
	GtkWidget *dialog, *widget_obj, *pan;

	disclaimer_text = dcgettext(NULL, "loca_ti_disclaimer", LC_MESSAGES);
	disclaimer_ok = dcgettext(NULL, "loca_bd_disclaimer_ok", LC_MESSAGES);
	disclaimer_reject =
	    dcgettext(NULL, "loca_bd_disclaimer_reject", LC_MESSAGES);

	/* TODO: what is 42 below? */
	dialog = gtk_dialog_new_with_buttons(disclaimer_text, NULL,
					     GTK_DIALOG_NO_SEPARATOR,
					     disclaimer_ok, GTK_RESPONSE_OK,
					     disclaimer_reject, 42, NULL);

	fi_disclaimer_text = dcgettext(NULL, "loca_fi_disclaimer", LC_MESSAGES);

	widget_obj = g_object_new(gtk_label_get_type(), "justify", 0,
				  "xalign", 0, 0, "yalign", 0, 0, 0, "wrap", 1,
				  "label", fi_disclaimer_text, NULL);
	gtk_widget_set_name(widget_obj, "osso-SmallFont");

	pan = hildon_pannable_area_new();
	hildon_pannable_area_add_with_viewport(HILDON_PANNABLE_AREA(pan),
					       widget_obj);
	g_object_set(pan, "hscrollbar-policy", 2, NULL);
	gtk_widget_set_size_request(pan, -1, 350);

	gtk_box_pack_start(GTK_BOX(dialog), pan, FALSE, TRUE, 0);
	gtk_widget_show_all(dialog);
	return dialog;
}

GtkWidget *create_positioning_dialog(void)
{
	char *gps_on_text, *done_text, *gps_text, *net_text;
	GtkWidget *dialog, *cb_gps, *cb_net;

	gps_on_text =
	    dcgettext(NULL, "loca_ti_switch_gps_network_on", LC_MESSAGES);
	done_text = dcgettext("hildon-libs", "wdgt_bd_done", LC_MESSAGES);

	dialog = gtk_dialog_new_with_buttons(gps_on_text, NULL,
					     GTK_DIALOG_NO_SEPARATOR, done_text,
					     GTK_RESPONSE_OK, NULL);

	cb_gps = hildon_check_button_new(HILDON_SIZE_FINGER_HEIGHT);
	gps_text = dcgettext(NULL, "loca_fi_gps", LC_MESSAGES);
	gtk_button_set_label(GTK_BUTTON(cb_gps), gps_text);
	gtk_box_pack_start(GTK_BOX(dialog), cb_gps, FALSE, FALSE, 0);
	gtk_widget_show(cb_gps);
	g_object_set_data(G_OBJECT(dialog), "gps-cb", cb_gps);

	cb_net = hildon_check_button_new(HILDON_SIZE_FINGER_HEIGHT);
	net_text = dcgettext(NULL, "loca_fi_network", LC_MESSAGES);
	gtk_button_set_label(GTK_BUTTON(cb_net), net_text);
	gtk_box_pack_start(GTK_BOX(dialog), cb_net, FALSE, FALSE, 0);
	gtk_widget_show(cb_gps);
	g_object_set_data(G_OBJECT(dialog), "net-cb", cb_net);

	return dialog;
}

GtkWidget *create_enable_gps_dialog(void)
{
	char *text = dcgettext(0, "loca_nc_switch_gps_on", LC_MESSAGES);
	return hildon_note_new_confirmation(NULL, text);
}

GtkWidget *create_enable_network_dialog(void)
{
	char *text = dcgettext(0, "loca_nc_switch_network_on", LC_MESSAGES);
	return hildon_note_new_confirmation(NULL, text);
}

GtkWidget *create_agnss_dialog(void)
{
	char *text =
	    dcgettext(NULL, "loca_nc_switch_network_and_gps_on", LC_MESSAGES);
	return hildon_note_new_confirmation(NULL, text);
}

GtkWidget *create_bt_disconnect_dialog(void)
{
	char *text = dcgettext(NULL, "loca_nc_bt_reconnect", LC_MESSAGES);
	return hildon_note_new_confirmation(NULL, text);
}

GtkWidget *create_privacy_verification_dialog(DBusMessage * msg,
					      DBusError * err)
{
	GtkWidget *ret;
	int accepted;
	char **arr;
	int arrlen;
	char *text, *unk_text;
	gchar *text_dup;

	if (!dbus_message_get_args
	    (msg, err, DBUS_TYPE_INT32, &accepted, DBUS_TYPE_ARRAY,
	     DBUS_TYPE_STRING, &arr, &arrlen, DBUS_TYPE_INVALID))
		return NULL;

	if (arrlen != 2) {
		dbus_set_error(err, "org.freedesktop.DBus.Error.Failed",
			       "Provide requestor and client");
		return NULL;
	}

	switch (accepted) {
	case 0:
		text =
		    dcgettext(NULL, "loca_nc_request_default_reject",
			      LC_MESSAGES);
		break;
	case 1:
		text =
		    dcgettext(NULL, "loca_nc_request_default_accept",
			      LC_MESSAGES);
		break;
	case -1:
		text =
		    dcgettext(NULL, "loca_nc_request_no_default", LC_MESSAGES);
		break;
	default:
		return NULL;
	}

	/* TODO: review */
	if (**arr)
		unk_text = *arr;
	else
		unk_text = dcgettext(NULL, "loca_va_unknown", LC_MESSAGES);

	text_dup = g_strdup_printf(text, unk_text);
	ret = hildon_note_new_confirmation(NULL, text_dup);
	g_free(text_dup);
	return ret;
}

GtkWidget *create_privacy_information_dialog(DBusMessage * msg, DBusError * err)
{
	GtkWidget *ret;
	int unused;
	char **arr;
	int arrlen;
	char *text, *unk_text;
	gchar *text_dup;

	if (!dbus_message_get_args
	    (msg, err, DBUS_TYPE_INT32, &unused, DBUS_TYPE_ARRAY,
	     DBUS_TYPE_STRING, &arr, &arrlen, DBUS_TYPE_INVALID))
		return NULL;

	if (arrlen != 2) {
		dbus_set_error(err, "org.freedesktop.DBus.Error.Failed",
			       "Provide requestor and client");
		return NULL;
	}

	text = dcgettext(NULL, "loca_ni_req_sent", LC_MESSAGES);

	/* TODO: review */
	if (**arr)
		unk_text = *arr;
	else
		unk_text = dcgettext(NULL, "loca_va_unknown", LC_MESSAGES);

	text_dup = g_strdup_printf(text, unk_text);
	ret = hildon_note_new_information(NULL, text_dup);
	g_free(text_dup);
	return ret;
}

GtkWidget *create_privacy_timeout_dialog(DBusMessage * msg, DBusError * err)
{
	GtkWidget *ret;
	int accepted;
	char **arr;
	int arrlen;
	char *text, *unk_text;
	gchar *text_dup;

	if (!dbus_message_get_args
	    (msg, err, DBUS_TYPE_INT32, &accepted, DBUS_TYPE_ARRAY,
	     DBUS_TYPE_STRING, &arr, &arrlen, DBUS_TYPE_INVALID))
		return NULL;

	if (arrlen != 2) {
		dbus_set_error(err, "org.freedesktop.DBus.Error.Failed",
			       "Provide requestor and client");
		return NULL;
	}

	if (accepted)
		text = dcgettext(NULL, "loca_ni_accepted", LC_MESSAGES);
	else
		text = dcgettext(NULL, "loca_ni_rejected", LC_MESSAGES);

	/* TODO: review */
	if (**arr)
		unk_text = *arr;
	else
		unk_text = dcgettext(NULL, "loca_va_unknown", LC_MESSAGES);

	text_dup = g_strdup_printf(text, unk_text);
	ret = hildon_note_new_information(NULL, text_dup);
	g_free(text_dup);
	return ret;
}

GtkWidget *create_privacy_expired_dialog(DBusMessage * msg, DBusError * err)
{
	gboolean accepted;
	char **arr;
	int arrlen;
	char *text;

	if (!dbus_message_get_args(msg, err, DBUS_TYPE_BOOLEAN, &accepted,
				   DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &arr,
				   &arrlen, DBUS_TYPE_INVALID))
		return NULL;

	if (arrlen != 2) {
		dbus_set_error(err, "org.freedesktop.DBus.Error.Failed",
			       "Provide requestor and client");
		return NULL;
	}
	if (accepted)
		text = dcgettext(NULL, "loca_ni_accept_expired", LC_MESSAGES);
	else
		text = dcgettext(NULL, "loca_ni_reject_expired", LC_MESSAGES);

	return hildon_note_new_information(NULL, text);
}

GtkWidget *create_default_supl_dialog(DBusMessage * msg, DBusError * err)
{
	GtkWidget *ret;
	char *res, *text;
	gchar *text_dup;

	if (!dbus_message_get_args
	    (msg, err, DBUS_TYPE_STRING, &res, DBUS_TYPE_INVALID))
		return NULL;

	text = dcgettext(NULL, "loca_in_default_supl_used", LC_MESSAGES);
	text_dup = g_strdup_printf(text, res);
	ret = hildon_note_new_information(NULL, text_dup);
	g_free(text_dup);
	return ret;

}

GtkWidget *create_bt_disabled_dialog(void)
{
	return hildon_note_new_information(NULL,
					   dcgettext("osso-connectivity-ui",
						     "conn_ib_not_available_bt_off",
						     LC_MESSAGES));
}

int compare_dialog_path(location_ui_t * location_ui, const char *path)
{
	return strcmp((const char *)location_ui->current_dialog, path);
}

DBusMessage *location_ui_close_dialog(location_ui_t * location_ui, GList * list,
				      DBusMessage * msg)
{
	dialog_data_t *dialog_data;
	int dialog_active, note_type;
	GList *dialogs;
	char *dbus_obj_path;
	DBusMessage *new_msg;

	dialog_data = list->data;
	dialog_active = dialog_data->dialog_active;
	new_msg = dbus_message_new_method_return(msg);
	dbus_message_append_args(new_msg, DBUS_TYPE_INT32,
				 &dialog_data->dialog_response_code,
				 DBUS_TYPE_INVALID);
	/* TODO: Review */
	if (dialog_data->window) {
		if (HILDON_IS_NOTE(dialog_data->window)) {
			note_type = 0;
			g_object_get(G_OBJECT(dialog_data->window), "note-type",
				     &note_type, 0);
			if ((unsigned int)(note_type - 2) > 1)
				gtk_widget_destroy(GTK_WIDGET
						   (dialog_data->window));
		} else {
			gtk_widget_destroy(GTK_WIDGET(dialog_data->window));
		}
		dialog_data->window = NULL;
	}

	if (dialog_data->foo2) {
		dialog_data->dialog_active = 0;
		dialog_data->maybe_path = 0;
		dialog_data->dialog_response_code = -1;
	} else {
		dialogs = g_list_delete_link(location_ui->dialogs, list);
		dbus_obj_path = dialog_data->dbus_object_path;
		location_ui->dialogs = dialogs;
		dbus_connection_unregister_object_path(location_ui->dbus,
						       dbus_obj_path);
		g_free(dialog_data->dbus_object_path);
		g_slice_free1(24u, dialog_data);	/* (sizeof(dialog_data), dialog_data) ? */
	}
	if (dialog_active == 2) {
		if (location_ui->current_dialog != dialog_data)
			g_assert("location_ui->current_dialog == dialog_data");
		location_ui->current_dialog = NULL;
		schedule_new_dialog(location_ui);
	}
	return new_msg;
}

DBusMessage *location_ui_display_dialog(location_ui_t * location_ui,
					GList * list, DBusMessage * msg)
{
	dbus_bool_t dbus_ret;
	dialog_data_t *dialog_data;
	int dialog_active_or_in_use;
	gboolean have_no_dialog;
	char *maybe_path;

	/* TODO: Review maybe_path */
	dbus_ret =
	    dbus_message_get_args(msg, NULL, DBUS_TYPE_INT32, &maybe_path,
				  DBUS_TYPE_INVALID);
	dialog_data = list->data;
	dialog_active_or_in_use = dialog_data->dialog_active;
	if (!dbus_ret)
		maybe_path = 0;
	if (dialog_active_or_in_use)
		return dbus_message_new_error_printf(msg,
						     "com.nokia.Location.UI.Error.InUse",
						     "%d",
						     dialog_data->dialog_response_code);

	have_no_dialog = location_ui->current_dialog == NULL;
	dialog_data->maybe_path = maybe_path;
	dialog_data->dialog_active = 1;
	if (have_no_dialog)
		schedule_new_dialog(location_ui);
	return dbus_message_new_method_return(msg);
}

int find_dbus_cb(int unused, DBusMessage * in_msg, location_ui_t * location_ui)
{
	int cnt, idx;
	const char *member, *message_path;
	GList *dialog_entry;
	gboolean found;
	DBusMessage *out_msg;

	cnt = 0;
	member = dbus_message_get_member(in_msg);
	message_path = dbus_message_get_path(in_msg);
	while (1) {
		/* TODO: review */
		found = g_str_equal(member, (&display_close_map)[2 * cnt]);
		idx = 2 * cnt++;
		if (found)
			break;
		if (cnt == 2)
			return 1;
	}
	/* Lookup function in previously created GList with path */
	dialog_entry = g_list_find_custom(location_ui->dialogs, message_path,
					  (GCompareFunc) compare_dialog_path);
	if (dialog_entry) {
		/* TODO: Review */
		out_msg =
		    display_close_map[idx + 1] (location_ui, dialog_entry,
						in_msg);
	} else {
		out_msg =
		    dbus_message_new_error(in_msg,
					   "org.freedesktop.DBus.Error.Failed",
					   "Bad object");
	}

	dbus_connection_send(location_ui->dbus, out_msg, NULL);
	dbus_connection_flush(location_ui->dbus);
	dbus_message_unref(out_msg);
	return 0;
}

int on_inactivity_timeout(location_ui_t * location_ui)
{
	if (location_ui->current_dialog)
		g_assert("location_ui->current_dialog == NULL");
	if (find_next_dialog(location_ui))
		g_assert("find_next_dialog(location_ui) == NULL");
	gtk_main_quit();
	return 0;
}

void on_dialog_response(GtkWidget * dialog, int gtk_response,
			location_ui_t * location_ui)
{
	dialog_data_t *item;
	GTypeInstance *gps_cb_data, *net_cb_data;
	GType hildon_checkbox_type;
	HildonCheckButton *net_cb_button, *gps_cb_button;
	gboolean net_button_active, gps_button_active;
	int gps_active_status, net_active_status, resp_code;
	DBusMessage *msg;
	gpointer cur_dialog;

	item = g_object_get_data(G_OBJECT(dialog), "dialog-data");
	if (!item)
		g_assert("item != NULL");

	item->dialog_response_code = 0;
	gps_cb_data = g_object_get_data(G_OBJECT(dialog), "gps-cb");
	net_cb_data = g_object_get_data(G_OBJECT(dialog), "net-cb");
	if (gps_cb_data && net_cb_data) {
		hildon_checkbox_type = hildon_check_button_get_type();
		net_cb_button = (HildonCheckButton *)
		    g_type_check_instance_cast(net_cb_data,
					       hildon_checkbox_type);
		net_button_active =
		    hildon_check_button_get_active(net_cb_button);
		gps_cb_button = (HildonCheckButton *)
		    g_type_check_instance_cast(gps_cb_data,
					       hildon_checkbox_type);
		gps_button_active =
		    hildon_check_button_get_active(gps_cb_button);
		gps_active_status = item->dialog_response_code;
		if (net_button_active)
			net_active_status = 2;
		else
			net_active_status = 0;
		if (gps_button_active)
			gps_active_status |= 1u;
		item->dialog_response_code =
		    gps_active_status | net_active_status;
	} else if (gtk_response == GTK_RESPONSE_OK) {
		/* ok/accepted */
		item->dialog_response_code = 0;
	} else {
		if (gtk_response == GTK_RESPONSE_CANCEL)
			resp_code = 1;
		else
			/* unknown/invalid ? */
			resp_code = -1;
		item->dialog_response_code = resp_code;
	}

	msg =
	    dbus_message_new_signal(item->dbus_object_path, LUI_DBUS_DIALOG,
				    "response");
	dbus_message_append_args(msg, DBUS_TYPE_INT32,
				 &item->dialog_response_code,
				 DBUS_TYPE_INVALID);
	dbus_connection_send(location_ui->dbus, msg, NULL);
	dbus_connection_flush(location_ui->dbus);
	dbus_message_unref(msg);
	gtk_widget_hide(dialog);
	cur_dialog = location_ui->current_dialog;
	item->dialog_active = 3;
	if (cur_dialog == item) {
		location_ui->current_dialog = NULL;
		schedule_new_dialog(location_ui);
	}
}

location_ui_dialog *find_next_dialog(location_ui_t * location_ui)
{
	GList *dialog;
	location_ui_dialog *next_dialog, *v4;
	gint32 cur_priority;

	dialog = location_ui->dialogs;
	if (!dialog)
		return NULL;

	next_dialog = NULL;
	/* I think this should be -1 */
	cur_priority = 0x80000000;

	do {
		while (1) {
			v4 = (location_ui_dialog *) dialog->data;
			if (v4->state == STATE_QUEUE)
				break;
			dialog = (GList *) dialog[1].data;
			if (!dialog)
				return next_dialog;
		}
		dialog = (GList *) dialog[1].data;
		if (v4->maybe_priority > cur_priority) {
			next_dialog = v4;
			cur_priority = v4->maybe_priority;
		}
	} while (dialog);
	return next_dialog;
}

void schedule_new_dialog(location_ui_t * location_ui)
{
	location_ui_dialog *dialog;
	char *destroy_data;
	int dialog_instance;
	gboolean dialog_instantiated;

	if (location_ui->current_dialog)
		g_assert("location_ui->current_dialog == NULL");

	dialog = find_next_dialog(location_ui);
	location_ui->current_dialog = dialog;
	if (dialog) {
		if (dialog->state != STATE_QUEUE)
			g_assert
			    ("location_ui->current_dialog->state == STATE_QUEUE");

		destroy_data = dialog->maybe_dialog_instance;
		dialog_instantiated = dialog->maybe_dialog_instance == NULL;
		dialog->state = STATE_2;
		if (dialog_instantiated) {
			if (!dialog->get_instance)
				g_assert
				    ("location_ui->current_dialog->get_instance != NULL");

			dialog_instance = dialog->get_instance();
			/* TODO: check this cast */
			dialog->maybe_dialog_instance = (char *)dialog_instance;
			g_object_set_data(G_OBJECT(location_ui->current_dialog),
					  "dialog-data",
					  location_ui->current_dialog);
			g_signal_connect_data(location_ui->current_dialog,
					      "response",
					      (GCallback) on_dialog_response,
					      location_ui,
					      (GClosureNotify) destroy_data,
					      (GConnectFlags) destroy_data);
		}

		gtk_window_present(GTK_WINDOW(location_ui->current_dialog));
		if (location_ui->inactivity_timeout_id) {
			g_source_remove(location_ui->inactivity_timeout_id);
			location_ui->inactivity_timeout_id = 0;
		}
	} else if (!location_ui->inactivity_timeout_id) {
		location_ui->inactivity_timeout_id = g_timeout_add(15000u,
								   (GSourceFunc)
								   on_inactivity_timeout,
								   location_ui);
	}
}

int main(int argc, char **argv, char **envp)
{
	int cnt = 0, prevcnt = 0, funccnt = 0;
	const char *path;
	location_ui_t location_ui;

	setlocale(LC_ALL, "");
	bindtextdomain("osso-location-ui", "/usr/share/locale");
	bind_textdomain_codeset("osso-location-ui", "UTF-8");
	textdomain("osso-location-ui");
	gtk_init(&argc, &argv);

	location_ui.dialogs = NULL;
	location_ui.current_dialog = NULL;
	location_ui.dbus = NULL;
	location_ui.inactivity_timeout_id = 0;

	location_ui.dbus = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (!location_ui.dbus) {
		g_printerr("Failed to init DBus\n");
		return 1;
	}

	dbus_connection_setup_with_g_main(location_ui.dbus, NULL);
	if (dbus_bus_request_name(location_ui.dbus, LUI_DBUS_NAME, 0, NULL) !=
	    1) {
		g_printerr
		    ("Failed to register service '%s'. Already running?\n",
		     LUI_DBUS_NAME);
		return 1;
	}

	if (!dbus_connection_register_object_path
	    (location_ui.dbus, LUI_DBUS_PATH, &vtable, &location_ui)) {
		g_printerr("Failed to register object\n");
		return 1;
	}

	do {
		location_ui.dialogs =
		    g_list_append(location_ui.dialogs, &funcmap[prevcnt]);
		++cnt;
		path = funcmap[funccnt].path;
		++funccnt;
		dbus_connection_register_object_path(location_ui.dbus, path,
						     &find_callback_vtable,
						     &location_ui);
		prevcnt = cnt;
	} while (cnt != 7);

	schedule_new_dialog(&location_ui);
	gtk_main();
	return 0;
}
