AC_PREREQ[(2.69])
AC_INIT([location-ui], [0.96], [parazyd@dyne.org])
AC_CONFIG_SRCDIR([src])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIRS([m4])

AC_PROG_CC
AC_PROG_INSTALL

PKG_CHECK_MODULES(UI, glib-2.0 dbus-1 dbus-glib-1 gtk+-2.0 hildon-1)
AC_SUBST(UI_CFLAGS)
AC_SUBST(UI_LIBS)

AC_ARG_ENABLE([maemo-launcher],
    [AS_HELP_STRING([--enable-maemo-launcher],
    [build with maemo-launcher support])],
    [case "${enableval}" in
    yes) maemo_launcher=true
         PKG_CHECK_MODULES(MAEMO_LAUNCHER, [maemo-launcher-app])
         AC_SUBST(MAEMO_LAUNCHER_CFLAGS)
         MAEMO_LAUNCHER_LIBS="-Xcompiler \"$MAEMO_LAUNCHER_LIBS\""
         AC_SUBST(MAEMO_LAUNCHER_LIBS)
         ;;
    no)  maemo_launcher=false ;;
    *) AC_MSG_ERROR([bad value ${enableval} for --enable-maemo-launcher]) ;;
    esac], [maemo_launcher=true])

AC_OUTPUT([Makefile src/Makefile])
