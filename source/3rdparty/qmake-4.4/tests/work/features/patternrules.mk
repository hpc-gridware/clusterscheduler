
.PHONY: all

all: case.1 case.2 case.3 case.4

# We can't have this, due to "Implicit Rule Search Algorithm" step 5c
#xxx: void

# 1 - existing file
%.1: void ; @exit 1
%.1: work/features/patternrules.mk ; @exit 0

# 2 - phony
%.2: void ; @exit 1
%.2: 2.phony ; @exit 0
.PHONY: 2.phony

# 3 - implicit-phony
%.3: void ; @exit 1
%.3: 3.implicit-phony ; @exit 0

3.implicit-phony:

# 4 - explicitly mentioned file made by an implicit rule
%.4: void ; @exit 1
%.4: test.x ; @exit 0
%.x: ;
