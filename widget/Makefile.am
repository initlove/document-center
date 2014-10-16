CLEANFILES =

noinst_LTLIBRARIES = libdc-widget.la

AM_CPPFLAGS =							\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/common					\
	$(COMMON_CFLAGS)					\
	$(DOCUMENT_CENTER_CFLAGS)				\
	-DG_LOG_DOMAIN=\"DocumentCenter\"			\
	$(NULL)

libdc_widget_la_SOURCES = \
	dc-view.c dc-view.h \
	$(NULL)

libdc_widget_la_LIBADD =	\
	$(top_srcdir)/common/libdc-common.la \
	$(COMMON_LIBS)			\
	$(DOCUMENT_CENTER_LIBS)		\
	$(NULL)

