CC = gcc
CFLAGS = -Wall -g -Iinclude
SRC_DIR = src
BUILD_DIR = build
LIB_DIR = lib

# Source and object files
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS =  $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))

# Library files
LIBS = -L$(LIB_DIR) -lmprompt  # Link against your static library

# Target executable
TARGET = $(BUILD_DIR)/program

all: clean $(TARGET)

# Rule to link object files and create the executable
$(TARGET): $(OBJS)
	# $(CC) $(CFLAGS) -o $(TARGET) $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# Rule to compile .c files into .o files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	@echo making build directory...
	mkdir -p $(BUILD_DIR)

# Clean up build files
clean:
	rm -rf $(BUILD_DIR)
