/*
 * Copyright (c) 2020 Ivan J. <parazyd@dyne.org>
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
#include <stdlib.h>

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <glib/glib.h>
#include <gtk/gtk.h>
#include <libintl.h>

#define UI_SERVICE_NAME "com.nokia.Location.UI"
#define UI_OBJECT_PATH  "/com/nokia/location/ui"

typedef struct location_ui {
	GList *dialogs;
	gpointer current_dialog;
	DBusConnection *dbus;
	guint inactivity_timeout_id;
};

typedef struct location_ui_dialog {
	int field_0;
	const char *path;
	int (*get_instance)(void);
	int field_C;
	int field_10;
	int state;
};

int main(int argc, char **argv)
{
	return 0;
}
