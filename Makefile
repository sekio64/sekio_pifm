INSTALL=install

PIFM_FLAGS=-fomit-frame-pointer -O3 -std=c++11

all: gensstv pifm

.PHONY: all install disctlean clean

pifm: pifm.cpp
	$(CXX) $(CXXFLAGS) $(PIFM_FLAGS) pifm.cpp -o pifm

gensstv: gensstv.c
	$(CC) $(CFLAGS) -lm -lgd -lmagic  gensstv.c -o gensstv

install: pifm gensstv
	$(INSTALL) -m 755 pifm /usr/local/bin
	$(INSTALL) -m 755 gensstv /usr/local/bin

clean:
	rm -f *~ *.old core

distclean:
	rm -f pifm gensstv
