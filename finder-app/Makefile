# ===== Toolchain selection =====
# Set CROSS_COMPILE to a prefix like "aarch64-none-linux-gnu-" to use a cross toolchain.
# Example:
#   make CROSS_COMPILE=aarch64-none-linux-gnu-
# This will use aarch64-none-linux-gnu-gcc, etc.
CROSS_COMPILE ?=

# Compiler and tools derived from CROSS_COMPILE
CC     := $(CROSS_COMPILE)gcc
STRIP  := $(CROSS_COMPILE)strip
AR     := $(CROSS_COMPILE)ar

# ===== Project settings =====
TARGET := writer
SRC    := writer.c
OBJ    := $(SRC:.c=.o)

# ===== Flags =====
CFLAGS := -Wall -Wextra -O2 -std=c99
LDFLAGS :=
# If you ever need to link extra libs, add to LDLIBS, e.g. -lm
LDLIBS :=

# Optional: You can define a sysroot if your cross toolchain needs it
# Pass SYSROOT on the command line:
#   make CROSS_COMPILE=aarch64-none-linux-gnu SYSROOT=/path/to/sysroot
ifdef SYSROOT
CFLAGS  += --sysroot=$(SYSROOT)
LDFLAGS += --sysroot=$(SYSROOT)
endif

# ===== Default target =====
.PHONY: all
all: $(TARGET)

# Link the final binary
$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Compile source to object
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ===== Utilities =====
.PHONY: clean debug strip
clean:
	rm -f $(TARGET) $(OBJ)

# Build with debug symbols and no optimization
debug: CFLAGS := -Wall -Wextra -g -O0 -std=c99
debug: clean all

# Strip binary (useful for release builds on embedded)
strip: $(TARGET)
	$(STRIP) --strip-unneeded $(TARGET)