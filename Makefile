all::

ccan: FORCE
	@$(MAKE) --no-print-directory -C ccan
dirclean: clean
	@$(MAKE) --no-print-directory -C ccan clean

test : ccan
obj-test = main.o penny/debug.o
TARGETS = test
CFLAGS += -I. -Iccan
LDFLAGS += -lev -lccan -Lccan

include base.mk
