
vpath hello.c src
all: hello.c; $(info $@ from $^)
hello.c: ; $(info 1 $@)
src/hello.c: ; $(info 2 $@)
