
SHOW = $(patsubst --jobserver-auth=%,--jobserver-auth=<auth>,$(MAKEFLAGS))
recurse: ; @echo $@: "/$(SHOW)/"; $(MAKE) -f work/features/jobserver.mk all
all:;@echo $@: "/$(SHOW)/"
