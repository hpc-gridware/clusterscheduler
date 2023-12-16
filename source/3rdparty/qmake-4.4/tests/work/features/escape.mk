
ifdef NOESC
path = pre:
endif
ifdef ONEESC
path = pre\:
endif
ifdef TWOESC
path = pre\\:
endif

$(path)foo : ; @echo "touch ($@)"

foo\ bar: ; @echo "touch ($@)"

sharp: foo\#bar.ext
foo\#bar.ext: ; @echo "foo#bar.ext = ($@)"
