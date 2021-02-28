libexec_PROGRAMS = location-ui

location_ui_CFLAGS = \
	-Wall -ggdb \
	$(UI_CFLAGS) \
	$(MAEMO_LAUNCHER_CFLAGS)

location_ui_LDFLAGS = \
	-Wl,--as-needed \
    $(UI_LIBS) \
    $(MAEMO_LAUNCHER_LIBS)

location_ui_SOURCES = main.c
