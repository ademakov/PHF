
AM_CPPFLAGS = -I$(top_srcdir)
AM_CXXFLAGS = -Wall -Wextra -msse4.2

noinst_PROGRAMS = public-suffix-builder public-suffix-lookup

public_suffix_builder_SOURCES = \
  public-suffix-builder.cc \
  SpookyV2.cpp SpookyV2.h
public_suffix_builder_LDADD = -lidn

public_suffix_lookup_SOURCES = \
  public-suffix-lookup.cc public-suffix-tables.h \
  SpookyV2.cpp SpookyV2.h
nodist_public_suffix_lookup_SOURCES = public-suffix-tables.h
BUILT_SOURCES = public-suffix-tables.h
CLEANFILES = public-suffix-tables.h

public-suffix-tables.h: public-suffix-builder public_suffix_list.dat supplementary_list.dat
	./public-suffix-builder public_suffix_list.dat supplementary_list.dat >public-suffix-tables.h
