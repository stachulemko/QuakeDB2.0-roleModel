# Makefile for QuakeDB2.0-roleModel

# Compiler and flags
CC = gcc
CXX = g++
CFLAGS = -Wall -Wextra -O2 -std=c11
CXXFLAGS = -Wall -Wextra -O2 -std=c++17
LDFLAGS = 

# Directories
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
TEST_DIR = test

# Target executable
TARGET = $(BIN_DIR)/quakedb

# Source files
SOURCES = $(wildcard $(SRC_DIR)/*.c $(SRC_DIR)/*.cpp)
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(filter %.c,$(SOURCES))) \
          $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(filter %.cpp,$(SOURCES)))

# Test files
TEST_SOURCES = $(wildcard $(TEST_DIR)/*.c $(TEST_DIR)/*.cpp)
TEST_OBJECTS = $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/test_%.o,$(filter %.c,$(TEST_SOURCES))) \
               $(patsubst $(TEST_DIR)/%.cpp,$(BUILD_DIR)/test_%.o,$(filter %.cpp,$(TEST_SOURCES)))
TEST_TARGET = $(BIN_DIR)/test_runner

# Default target
.PHONY: all
all: $(TARGET)

# Build target executable
$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "Build complete: $(TARGET)"

# Compile C source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile C++ source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compile test C source files
$(BUILD_DIR)/test_%.o: $(TEST_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile test C++ source files
$(BUILD_DIR)/test_%.o: $(TEST_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Build tests
.PHONY: test
test: $(TEST_TARGET)
	@echo "Running tests..."
	@$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJECTS) $(filter-out $(BUILD_DIR)/main.o,$(OBJECTS)) | $(BIN_DIR)
	$(CXX) $(TEST_OBJECTS) $(filter-out $(BUILD_DIR)/main.o,$(OBJECTS)) -o $@ $(LDFLAGS)

# Create directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Clean build artifacts
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)/*.o
	rm -f $(TARGET) $(TEST_TARGET)
	@echo "Clean complete"

# Clean everything including directories
.PHONY: distclean
distclean: clean
	rm -rf $(BUILD_DIR) $(BIN_DIR)
	@echo "Distribution clean complete"

# Install target (customize as needed)
.PHONY: install
install: $(TARGET)
	@echo "Installing $(TARGET) to /usr/local/bin..."
	@install -m 755 $(TARGET) /usr/local/bin/
	@echo "Installation complete"

# Uninstall target
.PHONY: uninstall
uninstall:
	@echo "Uninstalling from /usr/local/bin..."
	@rm -f /usr/local/bin/quakedb
	@echo "Uninstall complete"

# Run the program
.PHONY: run
run: $(TARGET)
	@$(TARGET)

# Debug build
.PHONY: debug
debug: CFLAGS += -g -DDEBUG
debug: CXXFLAGS += -g -DDEBUG
debug: clean $(TARGET)
	@echo "Debug build complete"

# Help target
.PHONY: help
help:
	@echo "QuakeDB2.0-roleModel Makefile"
	@echo ""
	@echo "Available targets:"
	@echo "  all        - Build the project (default)"
	@echo "  clean      - Remove build artifacts"
	@echo "  distclean  - Remove all generated files and directories"
	@echo "  test       - Build and run tests"
	@echo "  debug      - Build with debug symbols"
	@echo "  run        - Build and run the program"
	@echo "  install    - Install the program to /usr/local/bin"
	@echo "  uninstall  - Remove the program from /usr/local/bin"
	@echo "  help       - Show this help message"
