#ifndef _LOCATION_UI_MAIN_H
#define _LOCATION_UI_MAIN_H
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <glib.h>
#include <gtk/gtk.h>

#include <hildon/hildon.h>

typedef struct dialog_data_t {
	GtkWindow *window;
	char *dbus_object_path;
	void *maybe_func;
	char *path;
	int some_dbus_arg;
	int dialog_response_code;
	int dialog_active;
} dialog_data_t;

typedef struct location_ui_dialog {
	/* TODO: this is probably some gobject pointer/instance */
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

typedef struct client_request_table {
    char* text;
    void* func; /* TODO: specify this as GtkWidget* (void) or so */
} client_request_table;


/* TODO: Figure out the different states */
enum {
	STATE_0,
	STATE_QUEUE,
	STATE_2,
};

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
static int on_inactivity_timeout(location_ui_t *);
static void on_dialog_response(GtkWidget *, int, location_ui_t *);
static location_ui_dialog *find_next_dialog(location_ui_t *);
static void schedule_new_dialog(location_ui_t *);
DBusHandlerResult find_dbus_cb(DBusConnection *conn, DBusMessage * in_msg, void* data);
DBusHandlerResult on_client_request(DBusConnection *conn, DBusMessage *msg, void* data);


#define UI_BT_DISCONNECTED_PATH "/com/nokia/location/ui/bt_disconnected"
#define UI_DISLAIMER_PATH "/com/nokia/location/ui/disclaimer"
#define UI_ENABLE_GPS_PATH "/com/nokia/location/ui/enable_gps"
#define UI_ENABLE_NETWORK_PATH "/com/nokia/location/ui/enable_network"
#define UI_ENABLE_POSITIONING_PATH "/com/nokia/location/ui/enable_positioning"
#define UI_ENABLE_AGNSS_PATH "/com/nokia/location/ui/enable_agnss"
#define UI_BT_DISABLED "/com/nokia/location/ui/bt_disabled"

static struct dialog_data_t funcmap[7] = {
    {NULL, NULL, create_bt_disconnect_dialog, UI_BT_DISCONNECTED_PATH, 0, 0xFFFFFFFF, 0},
    {NULL, NULL, create_disclaimer_dialog, UI_DISLAIMER_PATH, 0, 0xFFFFFFFF, 0},
    {NULL, NULL, create_enable_gps_dialog, UI_ENABLE_GPS_PATH, 0, 0xFFFFFFFF, 0},
    {NULL, NULL, create_enable_network_dialog, UI_ENABLE_NETWORK_PATH, 0, 0xFFFFFFFF, 0},
    {NULL, NULL, create_positioning_dialog, UI_ENABLE_POSITIONING_PATH, 0, 0xFFFFFFFF, 0},
    {NULL, NULL, create_agnss_dialog, UI_ENABLE_AGNSS_PATH, 0, 0xFFFFFFFF, 0},
    {NULL, NULL, create_bt_disconnect_dialog, UI_BT_DISABLED, 0, 0xFFFFFFFF, 0},

};

static struct client_request_table clireq_table [5] = {
	{"location_verification", create_privacy_verification_dialog},
	{"location_information", create_privacy_information_dialog},
	{"location_timeout", create_privacy_timeout_dialog},
	{"location_expired", create_privacy_expired_dialog},
	{"location_default_supl", create_default_supl_dialog},
};

/* https://dbus.freedesktop.org/doc/api/html/structDBusObjectPathVTable.html */
static DBusObjectPathVTable vtable = {NULL, on_client_request, NULL, NULL, NULL, NULL};
static DBusObjectPathVTable find_callback_vtable = {NULL, find_dbus_cb, NULL, NULL, NULL, NULL};



#endif /* _LOCATION_UI_MAIN_H */
