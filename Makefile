CC=gcc
CFLAGS=-Wall -O2 -D_GNU_SOURCE
LDFLAGS=-lssl -lcrypto

TARGET=fm
SOURCE=fm.c

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LDFLAGS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/

.PHONY: clean install
