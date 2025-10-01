# Hot-reloader Makefile

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L
DEBUG_CFLAGS = -Wall -Wextra -std=c99 -g -DDO_DEBUG -O0 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L
LDFLAGS = 

# Project information
PROJECT_NAME = hot-reload
VERSION = 1.0.0
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1

# Source files
SOURCES = hot-reload.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = $(PROJECT_NAME)

# Default target
all: $(TARGET)

# Build the main executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Debug build
debug: CFLAGS = $(DEBUG_CFLAGS)
debug: $(TARGET)

# Install the binary
install: $(TARGET)
	install -d $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)/$(TARGET)

# Uninstall the binary
uninstall:
	rm -f $(BINDIR)/$(TARGET)

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(TARGET)

# Clean everything including generated files
distclean: clean
	rm -f *.o *.a *.so

# Create a release tarball
dist: clean
	mkdir -p $(PROJECT_NAME)-$(VERSION)
	cp $(SOURCES) README.md Makefile $(PROJECT_NAME)-$(VERSION)/
	tar -czf $(PROJECT_NAME)-$(VERSION).tar.gz $(PROJECT_NAME)-$(VERSION)
	rm -rf $(PROJECT_NAME)-$(VERSION)

# Run tests (if you add any)
test: $(TARGET)
	@echo "No tests defined yet"

# Show help
help:
	@echo "Available targets:"
	@echo "  all       - Build the project (default)"
	@echo "  debug     - Build with debug symbols and debug output"
	@echo "  install   - Install the binary to $(BINDIR)"
	@echo "  uninstall - Remove the binary from $(BINDIR)"
	@echo "  clean     - Remove build artifacts"
	@echo "  distclean - Remove all generated files"
	@echo "  dist      - Create a release tarball"
	@echo "  test      - Run tests"
	@echo "  help      - Show this help message"

# Phony targets
.PHONY: all debug install uninstall clean distclean dist test help
