TEMPLATE = app
TARGET = zs

LIBS += -lzstd

DESTDIR = $$PWD

SOURCES += \
	zs.cpp

HEADERS += \
	zs.h
