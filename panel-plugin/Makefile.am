AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-DG_LOG_DOMAIN=\"xfce4-twitch-plugin\" \
	-DPACKAGE_LOCALE_DIR=\"$(localedir)\" \
	$(PLATFORM_CPPFLAGS)

#
# Twitch plugin
#
plugin_LTLIBRARIES = \
	libtwitch-plugin.la

plugindir = \
	$(libdir)/xfce4/panel/plugins

libtwitch_plugin_la_SOURCES = \
	twitch-plugin.c \
	twitch-plugin.h \
	twitch-plugin-dialogs.c \
	twitch-plugin-dialogs.h \
	twitch-api.c \
	twitch-api.h

libtwitch_plugin_la_CFLAGS = \
	$(LIBXFCE4UTIL_CFLAGS) \
	$(LIBXFCE4UI_CFLAGS) \
	$(LIBXFCE4PANEL_CFLAGS) \
	$(PLATFORM_CFLAGS) \
	$(CURL_CFLAGS) \
	$(JSONC_CFLAGS)

libtwitch_plugin_la_LDFLAGS = \
       -avoid-version \
       -module \
       -no-undefined \
       -export-symbols-regex '^xfce_panel_module_(preinit|init|construct)' \
       $(PLATFORM_LDFLAGS)

libtwitch_plugin_la_LIBADD = \
	$(LIBXFCE4UTIL_LIBS) \
	$(LIBXFCE4UI_LIBS) \
	$(LIBXFCE4PANEL_LIBS) \
	$(CURL_LIBS) \
	$(JSONC_LIBS)

#
# Desktop file
#
desktopdir =								\
	$(datadir)/xfce4/panel/plugins

desktop_DATA =								\
	twitch-plugin.desktop

@INTLTOOL_DESKTOP_RULE@

EXTRA_DIST =								\
	twitch-plugin.desktop.in

CLEANFILES =								\
	$(desktop_DATA)

# vi:set ts=8 sw=8 noet ai nocindent syntax=automake:
