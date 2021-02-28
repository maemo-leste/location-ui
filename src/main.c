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

/* macros */
#define nelem(x) (sizeof (x) / sizeof *(x))

#define LUI_DBUS_NAME    "com.nokia.Location.UI"
#define LUI_DBUS_DIALOG  LUI_DBUS_NAME".Dialog"
#define LUI_DBUS_PATH    "/com/nokia/location/ui"

/* enums */
enum {
	STATE_0,
	STATE_QUEUE,
	STATE_2,
};

typedef struct location_ui_dialog {
	char *path;
	GtkWidget *(*dialog_func)(void);
	GtkWindow *window;
	int state;
	int priority;
	int dialog_active;
	int dialog_response_code;
	int some_dbus_arg;
} location_ui_dialog;

typedef struct location_ui_t {
	GList *dialogs;
	location_ui_dialog *current_dialog;
	DBusConnection *dbus;
	guint inactivity_timeout_id;
} location_ui_t;

typedef struct client_request_table {
	char *text;
	GtkWidget *(*func)(DBusMessage *, DBusError *);
} client_request_table;

typedef struct display_close_map {
	const char *text;
	DBusMessage *(*func)(location_ui_t *, GList *, DBusMessage *);
} display_close_map;

/* function declarations */
static GtkWidget *create_privacy_verification_dialog(DBusMessage *,
						     DBusError *);
static GtkWidget *create_privacy_information_dialog(DBusMessage *, DBusError *);
static GtkWidget *create_privacy_timeout_dialog(DBusMessage *, DBusError *);
static GtkWidget *create_privacy_expired_dialog(DBusMessage *, DBusError *);
static GtkWidget *create_default_supl_dialog(DBusMessage *, DBusError *);
static GtkWidget *create_bt_disconnected_dialog(void);
static GtkWidget *create_disclaimer_dialog(void);
static GtkWidget *create_enable_gps_dialog(void);
static GtkWidget *create_enable_network_dialog(void);
static GtkWidget *create_positioning_dialog(void);
static GtkWidget *create_agnss_dialog(void);
static location_ui_dialog *find_next_dialog(location_ui_t *);
static int on_inactivity_timeout(location_ui_t *);
static void on_dialog_response(GtkWidget *, int, location_ui_t *);
static void schedule_new_dialog(location_ui_t *);
static DBusMessage *location_ui_display_dialog(location_ui_t *, GList *,
					       DBusMessage *);
static DBusMessage *location_ui_close_dialog(location_ui_t *, GList *,
					     DBusMessage *);
static int compare_dialog_path(location_ui_dialog *, const char *);
static DBusHandlerResult on_client_request(DBusConnection *, DBusMessage *,
					   gpointer data);
static DBusHandlerResult find_dbus_cb(DBusConnection *, DBusMessage *,
				      gpointer data);

/* variables */
static struct client_request_table clireq_table[5] = {
	{"location_verification", create_privacy_verification_dialog},
	{"location_information", create_privacy_information_dialog},
	{"location_timeout", create_privacy_timeout_dialog},
	{"location_expired", create_privacy_expired_dialog},
	{"location_default_supl", create_default_supl_dialog},
};

static struct location_ui_dialog funcmap[6] = {
	{"/com/nokia/location/ui/bt_disconnected",
	 create_bt_disconnected_dialog, NULL, 0, 0, 0, 0, 0},
	{"/com/nokia/location/ui/disclaimer",
	 create_disclaimer_dialog, NULL, 0, 0, 0, 0, 0},
	{"/com/nokia/location/ui/enable_gps",
	 create_enable_gps_dialog, NULL, 0, 0, 0, 0, 0},
	{"/com/nokia/location/ui/enable_network",
	 create_enable_network_dialog, NULL, 0, 0, 0, 0, 0},
	{"/com/nokia/location/ui/enable_positioning",
	 create_positioning_dialog, NULL, 0, 0, 0, 0, 0},
	{"/com/nokia/location/ui/enable_agnss",
	 create_agnss_dialog, NULL, 0, 0, 0, 0, 0},
};

static display_close_map dc_map[2] = {
	{"display", location_ui_display_dialog},
	{"close", location_ui_close_dialog},
};

static DBusObjectPathVTable client_vtable = {
	NULL, on_client_request, NULL, NULL, NULL, NULL,
};

static DBusObjectPathVTable find_cb_vtable = {
	NULL, find_dbus_cb, NULL, NULL, NULL, NULL,
};

/* function implementations */
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

GtkWidget *create_bt_disconnected_dialog(void)
{
	char *t = dcgettext(NULL, "loca_nc_bt_reconnect", LC_MESSAGES);
	return hildon_note_new_confirmation(NULL, t);
}

GtkWidget *create_disclaimer_dialog(void)
{
	char *disclaimer_text, *disclaimer_ok, *disclaimer_reject;
	char *fi_disclaimer_text;
	GtkWidget *dialog, *label, *pan;

	disclaimer_text = dcgettext(NULL, "loca_ti_disclaimer", LC_MESSAGES);
	disclaimer_ok = dcgettext(NULL, "loca_bd_disclaimer_ok", LC_MESSAGES);
	disclaimer_reject =
	    dcgettext(NULL, "loca_bd_disclaimer_reject", LC_MESSAGES);

	/* TODO: What is 42 below? */
	dialog = gtk_dialog_new_with_buttons(disclaimer_text, NULL,
					     GTK_DIALOG_NO_SEPARATOR,
					     disclaimer_ok, GTK_RESPONSE_OK,
					     disclaimer_reject, 42, NULL);

	fi_disclaimer_text = dcgettext(NULL, "loca_fi_disclaimer", LC_MESSAGES);
	label = gtk_label_new(fi_disclaimer_text);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_widget_set_name(label, "osso-SmallFont");

	pan = hildon_pannable_area_new();
	hildon_pannable_area_add_with_viewport(HILDON_PANNABLE_AREA(pan),
					       label);
	g_object_set(G_OBJECT(pan), "hscrollbar-policy", 2, NULL);
	gtk_widget_set_size_request(pan, -1, 350);

	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), pan, FALSE, TRUE,
			   0);
	gtk_widget_show_all(dialog);
	return dialog;
}

GtkWidget *create_enable_gps_dialog(void)
{
	char *t = dcgettext(NULL, "loca_nc_switch_gps_on", LC_MESSAGES);
	return hildon_note_new_confirmation(NULL, t);
}

GtkWidget *create_enable_network_dialog(void)
{
	char *t = dcgettext(NULL, "loca_nc_switch_network_on", LC_MESSAGES);
	return hildon_note_new_confirmation(NULL, t);
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
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), cb_gps, FALSE,
			   FALSE, 0);
	gtk_widget_show(cb_gps);
	g_object_set_data(G_OBJECT(dialog), "gps-cb", cb_gps);

	cb_net = hildon_check_button_new(HILDON_SIZE_FINGER_HEIGHT);
	net_text = dcgettext(NULL, "loca_fi_network", LC_MESSAGES);
	gtk_button_set_label(GTK_BUTTON(cb_net), net_text);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), cb_net, FALSE,
			   FALSE, 0);
	gtk_widget_show(cb_net);
	g_object_set_data(G_OBJECT(dialog), "net-cb", cb_net);

	return dialog;
}

GtkWidget *create_agnss_dialog(void)
{
	char *t =
	    dcgettext(NULL, "loca_nc_switch_network_and_gps_on", LC_MESSAGES);
	return hildon_note_new_confirmation(NULL, t);
}

location_ui_dialog *find_next_dialog(location_ui_t * location_ui)
{
	g_debug(G_STRFUNC);
	location_ui_dialog *next_dialog, *tmp_dialog;
	gint32 cur_priority;
	GList *dialog_list;

	dialog_list = location_ui->dialogs;
	if (!dialog_list) {
		g_debug("%s: dialog_list is 0", G_STRLOC);
		return NULL;
	}

	next_dialog = NULL;
	/* I think this should be -1 */
	cur_priority = 0x80000000;

	do {
		while (1) {
			tmp_dialog = dialog_list->data;
			if (tmp_dialog->state == STATE_QUEUE) {
				g_debug("it's state_queue");
				break;
			}

			g_debug("it's NOT state_queue");
			dialog_list = g_list_next(dialog_list);
			if (!dialog_list) {
				return next_dialog;
			}
		}
		dialog_list = g_list_next(dialog_list);
		if (tmp_dialog->priority > cur_priority) {
			next_dialog = tmp_dialog;
			cur_priority = tmp_dialog->priority;
		}
	} while (dialog_list);

	g_debug("%s final path: %s", G_STRFUNC, next_dialog->path);
	return next_dialog;
}

int on_inactivity_timeout(location_ui_t * location_ui)
{
	g_assert(location_ui->current_dialog == NULL);
	g_assert(find_next_dialog(location_ui) == NULL);
	gtk_main_quit();
	return 0;
}

void on_dialog_response(GtkWidget * dialog, int gtk_response,
			location_ui_t * location_ui)
{
	location_ui_dialog *item;
	GTypeInstance *gps_cb_data, *net_cb_data;
	int gps_active_status, net_active_status, resp_code;
	HildonCheckButton *gps_cb_button, *net_cb_button;
	gboolean gps_button_active, net_button_active;
	DBusMessage *msg;
	gpointer cur_dialog;

	item = g_object_get_data(G_OBJECT(dialog), "dialog-data");
	g_assert(dialog != NULL);

	item->dialog_response_code = 0;
	gps_cb_data = g_object_get_data(G_OBJECT(dialog), "gps-cb");
	net_cb_data = g_object_get_data(G_OBJECT(dialog), "net-cb");

	if (gps_cb_data && net_cb_data) {
		gps_cb_button = HILDON_CHECK_BUTTON(gps_cb_data);
		net_cb_button = HILDON_CHECK_BUTTON(net_cb_data);
		gps_button_active =
		    hildon_check_button_get_active(gps_cb_button);
		net_button_active =
		    hildon_check_button_get_active(net_cb_button);

		gps_active_status = item->dialog_response_code;

		if (gps_button_active)
			gps_active_status |= 1u;

		if (net_button_active)
			net_active_status = 2;
		else
			net_active_status = 0;

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
			/* TODO: At least gets triggered when /disclaimer is Rejected */
			resp_code = -1;
		item->dialog_response_code = resp_code;
	}

	g_message("%s: response=%d", G_STRFUNC, item->dialog_response_code);

	msg = dbus_message_new_signal(item->path, LUI_DBUS_DIALOG, "response");
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

void schedule_new_dialog(location_ui_t * location_ui)
{
	g_debug(G_STRFUNC);
	gpointer destroy_data;

	g_assert(location_ui->current_dialog == NULL);
	location_ui->current_dialog = find_next_dialog(location_ui);

	if (location_ui->current_dialog) {
		g_assert(location_ui->current_dialog->state == STATE_QUEUE);

		destroy_data = location_ui->current_dialog->window;
		location_ui->current_dialog->state = STATE_2;

		if (location_ui->current_dialog->window == NULL) {
			location_ui->current_dialog->window =
			    GTK_WINDOW(location_ui->current_dialog->
				       dialog_func());
			g_object_set_data(G_OBJECT
					  (location_ui->current_dialog->window),
					  "dialog-data",
					  location_ui->current_dialog);
			g_signal_connect_data(location_ui->
					      current_dialog->window,
					      "response",
					      (GCallback) on_dialog_response,
					      location_ui,
					      /* TODO: this seems wrong */
					      (GClosureNotify) destroy_data,
					      (GConnectFlags) destroy_data);
		}
		gtk_window_present(location_ui->current_dialog->window);

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

DBusMessage *location_ui_display_dialog(location_ui_t * location_ui,
					GList * list, DBusMessage * msg)
{
	int some_dbus_arg;
	location_ui_dialog *dialog;

	if (!dbus_message_get_args
	    (msg, NULL, DBUS_TYPE_INT32, &some_dbus_arg, DBUS_TYPE_INVALID))
		some_dbus_arg = 0;

	dialog = list->data;

	if (dialog->dialog_active)
		return dbus_message_new_error_printf(msg,
						     "com.nokia.Location.UI.Error.InUse",
						     "%d",
						     dialog->dialog_response_code);

	dialog->some_dbus_arg = some_dbus_arg;
	/* TODO: dialog_active and state is the same? */
	dialog->dialog_active = 1;
	dialog->state = 1;
	if (location_ui->current_dialog == NULL)
		schedule_new_dialog(location_ui);
	return dbus_message_new_method_return(msg);
}

DBusMessage *location_ui_close_dialog(location_ui_t * location_ui, GList * list,
				      DBusMessage * msg)
{
	location_ui_dialog *dialog;
	DBusMessage *new_msg;
	GList *dialogs;
	int note_type;
	char *dbus_obj_path;

	dialog = list->data;

	new_msg = dbus_message_new_method_return(msg);
	dbus_message_append_args(new_msg, DBUS_TYPE_INT32,
				 dialog->dialog_response_code,
				 DBUS_TYPE_INVALID);

	/* TODO: Review */
	if (dialog->window) {
		if (HILDON_IS_NOTE(dialog->window)) {
			note_type = 0;
			g_object_get(G_OBJECT(dialog->window), "note-type",
				     &note_type, NULL);
			if ((unsigned int)(note_type - 2) > 1)
				gtk_widget_destroy(GTK_WIDGET(dialog->window));
		} else {
			gtk_widget_destroy(GTK_WIDGET(dialog->window));
		}
		dialog->window = NULL;
	}

	if (dialog->dialog_func) {
		dialog->dialog_active = 0;
		dialog->some_dbus_arg = 0;
		dialog->dialog_response_code = -1;
	} else {
		dialogs = g_list_delete_link(location_ui->dialogs, list);
		dbus_obj_path = dialog->path;
		location_ui->dialogs = dialogs;
		dbus_connection_unregister_object_path(location_ui->dbus,
						       dbus_obj_path);
		g_free(dialog->path);
		g_slice_free1(24u, dialog);	/* TODO: (sizeof(location_ui_dialog), dialog) ? */
	}

	/* TODO: state? */
	if (dialog->dialog_active == 2) {
		g_assert(location_ui->current_dialog == dialog);
		location_ui->current_dialog = NULL;
		schedule_new_dialog(location_ui);
	}

	return new_msg;
}

int compare_dialog_path(location_ui_dialog * dialog, const char *path)
{
	return strcmp(dialog->path, path);
}

DBusHandlerResult on_client_request(DBusConnection * conn, DBusMessage * msg,
				    gpointer data)
{
	/* TODO: */
	g_message(G_STRFUNC);
	const char *member = dbus_message_get_member(msg);
	g_debug("member=%s", member);
	return 0;
#if 0
signed int __fastcall on_client_request(int a1, DBusMessage *msg, location_ui *locui)
{
  int vtable_idx; // r4
  const char *v5; // r6
  int v7; // r5
  dialog_data *v8; // r0
  dialog_data *dialog; // r4
  char *dialog_path; // r0
  GList *new_dialogs; // r0
  char *dbus_path; // r1
  GObject *v13; // r0
  DBusMessage *new_msg; // r5
  DBusMessage *method_call; // [sp+Ch] [bp-38h]
  DBusError error; // [sp+10h] [bp-34h]

  method_call = msg;
  vtable_idx = 0;
  v5 = dbus_message_get_member(msg);
  while ( !g_str_equal(v5, (&create_dialog_vtable)[2 * vtable_idx]) )
  {
    if ( ++vtable_idx == 5 )
      return 1;
  }
  dbus_error_init(&error);
  v7 = ((int (__fastcall *)(DBusMessage *, DBusError *))(&create_dialog_vtable)[2 * vtable_idx + 1])(
         method_call,
         &error);
  if ( v7 )
  {
    if ( dbus_error_is_set(&error) )
      g_assertion_message_expr(0, "main.c", 466, "on_client_request", "!dbus_error_is_set (&error)");
    v8 = (dialog_data *)g_slice_alloc0(0x18u);
    ++locui[1].dialogs;                         // not sure what this does
    dialog = v8;
    v8->window = (GtkWindow *)v7;
    dialog_path = g_strdup_printf("/com/nokia/location/ui/dialog%d");
    dialog->dialog_active = 0;
    dialog->dialog_response_code = -1;
    dialog->dbus_object_path = dialog_path;
    new_dialogs = g_list_append(locui->dialogs, dialog);
    dbus_path = dialog->dbus_object_path;
    locui->dialogs = new_dialogs;
    dbus_connection_register_object_path(locui->dbus, dbus_path, &find_callback_vtable, locui);
    v13 = (GObject *)g_type_check_instance_cast(
                       &dialog->window->bin.container.widget.object.parent_instance.g_type_instance,
                       0x50u);
    g_object_set_data(v13, "dialog-data", dialog);
    g_signal_connect_data(dialog->window, "response", (GCallback)on_dialog_response, locui, 0, 0);
    new_msg = dbus_message_new_method_return(method_call);
    dbus_message_append_args(new_msg, 111, &dialog->dbus_object_path, 0);
  }
  else
  {
    if ( !dbus_error_is_set(&error) )
      g_assertion_message_expr(0, "main.c", 490, "on_client_request", "dbus_error_is_set (&error)");
    new_msg = dbus_message_new_error(method_call, error.name, error.message);
    dbus_error_free(&error);
  }
  dbus_connection_send(locui->dbus, new_msg, 0);
  dbus_connection_flush(locui->dbus);
  dbus_message_unref(new_msg);
  return 0;
}
#endif
}

DBusHandlerResult find_dbus_cb(DBusConnection * conn, DBusMessage * in_msg,
			       gpointer data)
{
	location_ui_t *location_ui = (location_ui_t *) data;
	const char *member, *message_path;
	int i, idx = -1;
	DBusMessage *out_msg;
	GList *dialog_entry;

	member = dbus_message_get_member(in_msg);
	message_path = dbus_message_get_path(in_msg);

	g_debug("%s: member=%s; path=%s", G_STRFUNC, member, message_path);

	for (i = 0; i < nelem(dc_map); i++) {
		if (g_str_equal(member, dc_map[i].text)) {
			idx = i;
			break;
		}
	}
	if (idx < 0)
		return 1;

	dialog_entry = g_list_find_custom(location_ui->dialogs, message_path,
					  (GCompareFunc) compare_dialog_path);

	if (dialog_entry)
		out_msg = dc_map[idx].func(location_ui, dialog_entry, in_msg);
	else
		out_msg =
		    dbus_message_new_error(in_msg,
					   "org.freedesktop.DBus.Error.Failed",
					   "Bad object");

	dbus_connection_send(location_ui->dbus, out_msg, NULL);
	dbus_connection_flush(location_ui->dbus);
	dbus_message_unref(out_msg);
	return 0;
}

int main(int argc, char **argv, char **envp)
{
	int i;
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
	    (location_ui.dbus, LUI_DBUS_PATH, &client_vtable, &location_ui)) {
		g_printerr("Failed to register object\n");
		return 1;
	}

	for (i = 0; i < nelem(funcmap); i++) {
		location_ui.dialogs =
		    g_list_append(location_ui.dialogs, &funcmap[i]);
		path = funcmap[i].path;
		g_debug("Registering %s", path);
		dbus_connection_register_object_path(location_ui.dbus, path,
						     &find_cb_vtable,
						     &location_ui);
	}

	schedule_new_dialog(&location_ui);
	gtk_main();
	return 0;
}
