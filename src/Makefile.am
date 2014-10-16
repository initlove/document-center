AM_CPPFLAGS =                                                   \
	-I$(top_srcdir)                                         \
	-I$(top_srcdir)/common					\
	-I$(top_srcdir)/lib					\
	-I$(top_srcdir)/widget					\
	$(DOCUMENT_CENTER_CFLAGS)                               \
	-DPIXMAPSDIR=\""$(datadir)/pixmaps/@PACKAGE@\""		\
	-DG_LOG_DOMAIN=\"DocumentCenter\"                       \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale\""  \
	-DPNP_IDS=\""$(PNP_IDS)"\"                              \
	$(DEBUG_CFLAGS) 					\
	$(DISABLE_DEPRECATED_CFLAGS)

bin_PROGRAMS = document-center

noinst_PROGRAMS =      \
	$(NULL)

SOURCE_FILES =	\
	$(NULL)

document_center_SOURCES =        \
	$(SOURCE_FILES)		\
	main.c

document_center_LDADD =          \
	$(top_srcdir)/widget/libdc-widget.la \
	$(DOCUMENT_CENTER_LIBS)

