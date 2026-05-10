# Makefile for Custom Memory Allocator Lab
#
# Targets:
#   make          - Build student implementation
#   make test     - Run functional tests
#   make lib      - Build shared library (libmymalloc.so)
#   make clean    - Clean build artifacts
#   make help     - Show this help message

CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude
LDFLAGS = -lpthread

# Optimization flags for release
CFLAGS_OPT = -Wall -Wextra -O2 -Iinclude

# Flags for shared library
CFLAGS_LIB = -Wall -Wextra -O2 -fPIC -Iinclude -DUSE_STANDARD_NAMES
LDFLAGS_LIB = -shared

# Directories
SRC_DIR = src
TEST_DIR = tests
INCLUDE_DIR = include

# Source files
STUDENT_SRC = $(SRC_DIR)/my_malloc.c

# Test files
TEST_FUNC_SRC = $(TEST_DIR)/test_functional.c

# Output binaries
TEST_FUNC_BIN = test_functional

# Shared library
LIB_NAME = libmymalloc.so
LIB_SOL_NAME = libmymalloc_solution.so

# Default target
.PHONY: all
all: $(TEST_FUNC_BIN)

# Build functional test with student implementation
$(TEST_FUNC_BIN): $(TEST_FUNC_SRC) $(STUDENT_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Build shared library with standard malloc/free names
$(LIB_NAME): $(STUDENT_SRC)
	$(CC) $(CFLAGS_LIB) $(LDFLAGS_LIB) -o $@ $^

# Build shared library
.PHONY: lib
lib: $(LIB_NAME)
	ln -sf $(LIB_NAME) $(LIB_SOL_NAME)
	@echo "============================================"
	@echo "Built shared library: $(LIB_NAME)"
	@echo "Compatibility alias: $(LIB_SOL_NAME)"
	@echo "Usage: LD_PRELOAD=./$(LIB_NAME) <program>"
	@echo "============================================"

# Run functional tests
.PHONY: test
test: $(TEST_FUNC_BIN)
	@echo "============================================"
	@echo "Running Functional Tests..."
	@echo "============================================"
	./$(TEST_FUNC_BIN)

# Clean build artifacts
.PHONY: clean
clean:
	rm -f $(TEST_FUNC_BIN) $(TEST_FUNC_SOL_BIN)
	rm -f $(LIB_NAME) $(LIB_SOL_NAME)
	rm -f *.o $(SRC_DIR)/*.o $(TEST_DIR)/*.o
	rm -f core core.*

# Generate compile_commands.json for IDE support
.PHONY: compile_commands
compile_commands:
	bear -- make clean all

# Help message
.PHONY: help
help:
	@echo "Custom Memory Allocator Lab - Build System"
	@echo ""
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@echo "  all              Build student implementation (default)"
	@echo "  test             Run functional tests"
	@echo "  lib              Build shared library (libmymalloc.so)"
	@echo "  clean            Remove build artifacts"
	@echo "  help             Show this help message"
	@echo ""
	@echo "Performance Testing:"
	@echo "  Use benchmark.sh for glibc benchtests integration"
	@echo "  ./benchmark.sh --help    Show benchmark script usage"
	@echo ""
	@echo "Example:"
	@echo "  make && make test        # Build and run functional tests"
	@echo "  make lib                 # Build shared library"
	@echo "  LD_PRELOAD=./libmymalloc.so ls  # Use custom malloc with any program"
