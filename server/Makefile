# ===== Toolchain selection =====
# Set CROSS_COMPILE to a prefix like "aarch64-none-linux-gnu-" to use a cross toolchain.
# Example:
#   make CROSS_COMPILE=aarch64-none-linux-gnu-
# This will use aarch64-none-linux-gnu-gcc, etc.
CROSS_COMPILE ?=

# Compiler and tools derived from CROSS_COMPILE
CC     ?= $(CROSS_COMPILE)gcc
STRIP  ?= $(CROSS_COMPILE)strip
AR     ?= $(CROSS_COMPILE)ar

# ===== Project settings =====
TARGET := aesdsocket
SRC    := aesdsocket.c
OBJ    := $(SRC:.c=.o)

# ===== Flags =====
USE_AESD_CHAR_DEVICE ?= 1
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -DUSE_AESD_CHAR_DEVICE=$(USE_AESD_CHAR_DEVICE)
LDFLAGS +=
# If you ever need to link extra libs, add to LDLIBS, e.g. -lm
LDLIBS +=

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
debug: CFLAGS += -Wall -Wextra -Werror -g -O0
debug: clean all

# Strip binary (useful for release builds on embedded)
strip: $(TARGET)
	$(STRIP) --strip-unneeded $(TARGET)