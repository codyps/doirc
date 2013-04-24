all::

obj-test = main.o penny/tcp.o
TARGETS = test
CFLAGS += -I.
LDFLAGS += -lev

include base.mk
