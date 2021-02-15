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
static int on_inactivity_timeout(location_ui_t *);
static void on_dialog_response(GtkWidget *, int, location_ui_t *);
static location_ui_dialog *find_next_dialog(location_ui_t *);
static void schedule_new_dialog(location_ui_t *);

/* variables */
static DBusObjectPathVTable vtable;
static DBusObjectPathVTable find_callback_vtable;
static struct dialog_data_t funcmap[7];

int on_inactivity_timeout(location_ui_t * location_ui)
{
	return 0;
}

void on_dialog_response(GtkWidget * dialog, int gtk_response,
			location_ui_t * location_ui)
{
	return;
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
