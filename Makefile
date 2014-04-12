all::

obj-irc = irc.o tommyds/tommyds/tommyhashlin.o tommyds/tommyds/tommyhash.o tommyds/tommyds/tommylist.o

obj-simple = test.o irc_helpers.o $(obj-irc)
obj-lunch-bot = lunch-bot.o irc_helpers.o user-track.o $(obj-irc)
TARGETS = lunch-bot simple
ALL_CFLAGS += -I. -Dtommy_inline="static inline" -Itommyds
ALL_LDFLAGS += -lev

include base.mk
include base-ccan.mk
