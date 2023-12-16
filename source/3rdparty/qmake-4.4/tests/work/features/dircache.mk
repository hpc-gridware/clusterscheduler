
_orig := $(wildcard ./*)
$(shell echo > anewfile)
_new := $(wildcard ./*)
$(info diff=$(filter-out $(_orig),$(_new)))
all:;@:
