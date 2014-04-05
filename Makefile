all::

obj-test = main.o irc.o penny/debug.o rbtree/rbtree.o
obj-lunch-bot = lunch-bot.o irc.o penny/debug.o
TARGETS = test lunch-bot
ALL_CFLAGS += -I.
ALL_LDFLAGS += -lev

include base.mk
include base-ccan.mk
