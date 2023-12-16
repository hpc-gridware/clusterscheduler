
EXPAND = expansion
all: ; @echo $(test-expand $$(EXPAND))
load testapi.so
