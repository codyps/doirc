all::

test : ccan
obj-test = main.o irc.o penny/debug.o rbtree/rbtree.o
TARGETS = test
ALL_CFLAGS += -I.
ALL_LDFLAGS += -lev

include base.mk
include base-ccan.mk
