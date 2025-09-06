
CC=gcc
CFLAGS=-Wall -O2 -D_GNU_SOURCE
LDFLAGS=-lssl -lcrypto

TARGET=fm
BUILDDIR=build
SOURCE=fm.c
BIN=$(BUILDDIR)/$(TARGET)

all: $(BUILDDIR) $(BIN) install

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BIN): $(SOURCE)
	$(CC) $(CFLAGS) -o $(BIN) $(SOURCE) $(LDFLAGS)

clean:
	rm -rf $(BUILDDIR)

install: $(BIN)
	sudo cp -f $(BIN) /usr/local/bin/

.PHONY: all clean install
