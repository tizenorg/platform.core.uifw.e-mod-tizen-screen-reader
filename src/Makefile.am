MAINTAINERCLEANFILES = Makefile.in
MODULE = e-mod-tizen-screen-reader

LDFLAGS +=

pkgdir                 = $(libdir)/enlightenment/modules/$(MODULE)/$(MODULE_ARCH)
pkg_LTLIBRARIES        = module.la
module_la_SOURCES      = \
	e_mod_main.c \
	e_screen_reader_config.c \
	e_screen_reader_gestures.c

#if HAVE_WAYLAND_ONLY
#module_la_SOURCES      += e_screen_reader_*.c \
#endif

module_la_LIBADD       =
module_la_CFLAGS       = @ENLIGHTENMENT_CFLAGS@
module_la_LDFLAGS      = -module -avoid-version @ENLIGHTENMENT_LIBS@
module_la_DEPENDENCIES = $(top_builddir)/config.h
