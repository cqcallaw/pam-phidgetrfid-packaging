pamdir = $(libdir)/security
pam_LTLIBRARIES = pam_phidgetrfid.la
pam_phidgetrfid_la_SOURCES = pam-phidgetrfid.c pam-phidgetrfid.h
pam_phidgetrfid_la_LDFLAGS = -version-info 1:2:0 -module -avoid-version -shared 
pam_phidgetrfid_la_CPPFLAGS = -I$(top_srcdir)/common
pam_phidgetrfid_la_LIBADD = ../common/libscanner-interface.la
