
all: hello
%: %.c; $(info hello.c)
%: %.f; $(info hello.f)
