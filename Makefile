CC = gcc
CFLAGS = -Wall -Iinclude -g

# Core source files
CORE_SRCS = src/phys.c src/pt.c src/mm.c src/core.c
CORE_OBJS = $(CORE_SRCS:.c=.o)

# Test files
TEST_DIR = tests
TEST_SRCS = $(wildcard $(TEST_DIR)/test_phase*.c)
TEST_TARGETS = $(TEST_SRCS:.c=)

.PHONY: all clean test test1 test2 test3 test4 test5 test6 test-all

all: tests

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build individual test
$(TEST_DIR)/test_phase%: $(TEST_DIR)/test_phase%.c $(CORE_OBJS)
	$(CC) $(CFLAGS) $< $(CORE_OBJS) -o $@

# Build all tests
tests: $(TEST_TARGETS)

# Run individual phase tests
test1: $(TEST_DIR)/test_phase1
	@echo "========================================"
	@echo "  Running Phase 1 Tests"
	@echo "========================================"
	@./$(TEST_DIR)/test_phase1

test2: $(TEST_DIR)/test_phase2
	@echo "========================================"
	@echo "  Running Phase 2 Tests"
	@echo "========================================"
	@./$(TEST_DIR)/test_phase2

test3: $(TEST_DIR)/test_phase3
	@echo "========================================"
	@echo "  Running Phase 3 Tests"
	@echo "========================================"
	@./$(TEST_DIR)/test_phase3

test4: $(TEST_DIR)/test_phase4
	@echo "========================================"
	@echo "  Running Phase 4 Tests"
	@echo "========================================"
	@./$(TEST_DIR)/test_phase4

test5: $(TEST_DIR)/test_phase5
	@echo "========================================"
	@echo "  Running Phase 5 Tests (Advanced)"
	@echo "========================================"
	@./$(TEST_DIR)/test_phase5

test6: $(TEST_DIR)/test_phase6
	@echo "========================================"
	@echo "  Running Phase 6 Tests (Advanced)"
	@echo "========================================"
	@./$(TEST_DIR)/test_phase6

# Run all tests
test-all: tests
	@echo "========================================"
	@echo "  Running All Phase Tests"
	@echo "========================================"
	@for t in $(TEST_TARGETS); do \
		echo ""; \
		echo ">>> Running $$t <<<"; \
		./$$t || true; \
	done
	@echo ""
	@echo "========================================"
	@echo "  All tests completed"
	@echo "========================================"

# Alias for test-all
test: test-all

clean:
	rm -f $(CORE_OBJS)
	rm -f $(TEST_DIR)/*.o $(TEST_TARGETS)
	rm compile_commands.json
