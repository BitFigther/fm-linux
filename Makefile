
TARGET=fm
BUILDDIR=build
BIN=$(BUILDDIR)/$(TARGET)
GOFILES=$(shell find . -name '*.go' -not -path './$(BUILDDIR)/*')

all: $(BUILDDIR) $(BIN)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

$(BIN): $(GOFILES) go.mod go.sum
	go build -o $(BIN) .

test:
	go test -v ./...

clean:
	rm -rf $(BUILDDIR)

install: $(BIN)
	sudo cp -f $(BIN) /usr/local/bin/

.PHONY: all clean install test
