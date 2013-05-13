all::

ccan: FORCE
	@$(MAKE) $(MAKEFLAGS) --no-print-directory -C ccan
dirclean: clean
	@$(MAKE) $(MAKEFLAGS) --no-print-directory -C ccan clean

test : ccan
obj-test = main.o irc.o penny/debug.o rbtree/rbtree.o
TARGETS = test
CFLAGS += -I. -Iccan
LDFLAGS += -lev -lccan -Lccan

include base.mk
