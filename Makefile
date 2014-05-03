all::

obj-tommy = tommyds/tommyds/tommyhashlin.o tommyds/tommyds/tommyhash.o tommyds/tommyds/tommylist.o
obj-irc = irc.o $(obj-tommy)

obj-simple = test.o irc_helpers.o $(obj-irc)
obj-lunch-bot = lunch-bot.o irc_helpers.o user-track.o $(obj-irc)
obj-test-iter = tommyhashlin-iter.o $(obj-tommy)
TARGETS = lunch-bot simple test-iter
ALL_CFLAGS += -I. -Dtommy_inline="static inline" -Itommyds
ALL_LDFLAGS += -lev

include base.mk
include base-ccan.mk
