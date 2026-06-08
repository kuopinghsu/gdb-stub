# Makefile for RISC-V Emulator with GDB Stub

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -g
LDFLAGS = 

# Directories
SRC_DIR = src
EMU_DIR = emu
BUILD_DIR = build

# Source files
GDB_STUB_SOURCES = $(SRC_DIR)/gdb_stub.c
EMU_SOURCES = $(EMU_DIR)/rv32_cpu.c $(EMU_DIR)/smp_sync.c $(EMU_DIR)/main.c
ALL_SOURCES = $(GDB_STUB_SOURCES) $(EMU_SOURCES)

# Object files
GDB_STUB_OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(GDB_STUB_SOURCES))
EMU_OBJECTS = $(patsubst $(EMU_DIR)/%.c, $(BUILD_DIR)/%.o, $(EMU_SOURCES))
ALL_OBJECTS = $(GDB_STUB_OBJECTS) $(EMU_OBJECTS)

# Headers
HEADERS = $(SRC_DIR)/gdb_stub.h $(EMU_DIR)/rv32_cpu.h $(EMU_DIR)/smp_sync.h $(EMU_DIR)/smp_program.h $(EMU_DIR)/rv32_system.h

# Target executable
TARGET = $(BUILD_DIR)/rv32_emu

# Default target
all: $(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build the emulator
$(TARGET): $(BUILD_DIR) $(ALL_OBJECTS)
	$(CC) $(ALL_OBJECTS) -o $@ $(LDFLAGS)
	@echo "Build complete: $(TARGET)"

# Compile GDB stub sources
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(EMU_DIR) -c $< -o $@

# Compile emulator sources
$(BUILD_DIR)/%.o: $(EMU_DIR)/%.c $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(EMU_DIR) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)
	@echo "Clean complete"

# Run the emulator
run: $(TARGET)
	./$(TARGET)

# Run with debug output
debug: $(TARGET)
	./$(TARGET) -d

# Run in interactive mode
interactive: $(TARGET)
	./$(TARGET) -i

# Run with custom GDB port
run-port: $(TARGET)
	./$(TARGET) -p 2345

# Create a simple test program in assembly
test-program:
	@echo "Creating test program..."
	@mkdir -p examples
	@echo ".section .text" > examples/test.s
	@echo ".globl _start" >> examples/test.s
	@echo "_start:" >> examples/test.s
	@echo "    lui  t0, 0x80000      # Load base address" >> examples/test.s
	@echo "    addi t1, zero, 0      # Counter = 0" >> examples/test.s
	@echo "    addi t2, zero, 10     # Limit = 10" >> examples/test.s
	@echo "    lui  s0, 0x10000      # UART base" >> examples/test.s
	@echo "loop:" >> examples/test.s
	@echo "    beq  t1, t2, end      # if counter == limit, exit" >> examples/test.s
	@echo "    addi t1, t1, 1        # counter++" >> examples/test.s
	@echo "    jal  loop             # jump back" >> examples/test.s
	@echo "end:" >> examples/test.s
	@echo "    ecall                 # exit" >> examples/test.s
	@echo "Test program created in examples/test.s"

# Run SMP tests for a given hart count (2, 4, or 8)
define RUN_SMP_TEST
	@echo "Running SMP tests ($(1) harts)..."
	@$(MAKE) stop-emu
	@./$(TARGET) -p 1234 -c $(1) & \
	EMU_PID=$$!; \
	sleep 2; \
	riscv-none-elf-gdb -batch -ex 'set $$expected_harts=$(1)' -x gdb_test_smp.gdb || { kill $$EMU_PID 2>/dev/null; $(MAKE) stop-emu; exit 1; }; \
	kill $$EMU_PID 2>/dev/null || $(MAKE) stop-emu
endef

# Run the complete demo (single-core + SMP tests and demos)
demo: clean $(TARGET)
	@echo "Starting emulator and GDB demo..."
	@echo "This will:"
	@echo "  1. Clean and build the project"
	@echo "  2. Run single-core automated tests and demo"
	@echo "  3. Run SMP automated tests (2, 4, 8 harts) and demo"
	@echo ""
	@$(MAKE) stop-emu
	@echo "=== Single-core tests ==="
	@./$(TARGET) -p 1234 -c 1 & \
	EMU_PID=$$!; \
	sleep 2; \
	riscv-none-elf-gdb -batch -x gdb_test.gdb || { kill $$EMU_PID 2>/dev/null; $(MAKE) stop-emu; exit 1; }; \
	kill $$EMU_PID 2>/dev/null; \
	$(MAKE) stop-emu
	@echo ""
	@echo "=== Single-core demo ==="
	@./$(TARGET) -p 1234 -c 1 & \
	EMU_PID=$$!; \
	sleep 2; \
	riscv-none-elf-gdb -batch -x gdb_demo.gdb examples/test.s; \
	kill $$EMU_PID 2>/dev/null; \
	$(MAKE) stop-emu
	@$(MAKE) test-smp
	@echo ""
	@echo "=== SMP demo (4 harts) ==="
	@./$(TARGET) -p 1234 -c 4 & \
	EMU_PID=$$!; \
	sleep 2; \
	riscv-none-elf-gdb -batch -ex 'set $$expected_harts=4' -x gdb_demo_smp.gdb examples/test.s; \
	kill $$EMU_PID 2>/dev/null || $(MAKE) stop-emu

# Run automated GDB tests only (single-core + SMP 2/4/8)
test: clean $(TARGET)
	@echo "Running automated GDB stub tests (single-core)..."
	@$(MAKE) stop-emu
	@./$(TARGET) -p 1234 -c 1 & \
	EMU_PID=$$!; \
	sleep 2; \
	riscv-none-elf-gdb -batch -x gdb_test.gdb; \
	TEST_EXIT=$$?; \
	kill $$EMU_PID 2>/dev/null || $(MAKE) stop-emu; \
	if [ $$TEST_EXIT -ne 0 ]; then exit $$TEST_EXIT; fi
	@$(MAKE) test-smp

# Run SMP tests for 2, 4, and 8 harts
test-smp: clean $(TARGET)
	$(call RUN_SMP_TEST,2)
	$(call RUN_SMP_TEST,4)
	$(call RUN_SMP_TEST,8)

# Start emulator in background for manual GDB connection
# Optional: make start-emu HARTS=4  (valid: 1, 2, 4, 8)
HARTS ?= 1
start-emu: $(TARGET)
	@echo "Starting emulator on port 1234 ($(HARTS) hart(s))..."
	@pkill -f rv32_emu || true
	@./$(TARGET) -p 1234 -c $(HARTS) &
	@echo "Emulator started in background. Connect with:"
	@echo "  riscv-none-elf-gdb -x gdb_demo.gdb examples/test.s"
	@echo "  riscv-none-elf-gdb -ex \"set \\\$$expected_harts=$(HARTS)\" -x gdb_demo_smp.gdb examples/test.s   # SMP"
	@echo "Or manually:"
	@echo "  riscv-none-elf-gdb"
	@echo "  (gdb) target remote localhost:1234"

# Stop the background emulator
stop-emu:
	@echo "Stopping emulator..."
	@pkill -f rv32_emu || true
	@echo "Emulator stopped"

# Connect to running emulator with GDB
gdb-connect:
	riscv-none-elf-gdb -x gdb_demo.gdb examples/test.s

# Help
help:
	@echo "Available targets:"
	@echo "  all           - Build the emulator (default)"
	@echo "  clean         - Remove build artifacts"
	@echo "  run           - Run the emulator with default settings"
	@echo "  debug         - Run with debug output enabled"
	@echo "  interactive   - Run in interactive mode"
	@echo "  run-port      - Run with custom GDB port (2345)"
	@echo "  test          - Build and run single-core + SMP automated tests"
	@echo "  test-smp      - Build and run SMP tests (2, 4, and 8 harts)"
	@echo "  demo          - Build, run tests, then demos (single-core + SMP)"
	@echo "  start-emu     - Start emulator in background on port 1234"
	@echo "  stop-emu      - Stop background emulator"
	@echo "  gdb-connect   - Connect to running emulator with GDB"
	@echo "  test-program  - Create a sample RISC-V assembly program"
	@echo "  help          - Show this help message"
	@echo ""
	@echo "Quick start:"
	@echo "  make demo     - Run automated tests and demos (single + SMP)"
	@echo "  make test     - Run automated tests only (single + SMP)"
	@echo "  make start-emu HARTS=8  - Start 8-hart SMP emulator (1, 2, 4, or 8)"
	@echo ""
	@echo "Manual workflow:"
	@echo "  make start-emu && make gdb-connect"
	@echo "  (or in separate terminals)"
	@echo ""
	@echo "To use with GDB manually:"
	@echo "  1. make start-emu"
	@echo "  2. In another terminal: riscv-none-elf-gdb"
	@echo "  3. (gdb) target remote localhost:1234"
	@echo "  4. (gdb) continue"

# Dependencies
$(BUILD_DIR)/gdb_stub.o: $(SRC_DIR)/gdb_stub.c $(SRC_DIR)/gdb_stub.h
$(BUILD_DIR)/rv32_cpu.o: $(EMU_DIR)/rv32_cpu.c $(EMU_DIR)/rv32_cpu.h
$(BUILD_DIR)/main.o: $(EMU_DIR)/main.c $(SRC_DIR)/gdb_stub.h $(EMU_DIR)/rv32_cpu.h $(EMU_DIR)/rv32_system.h $(EMU_DIR)/smp_sync.h $(EMU_DIR)/smp_program.h
$(BUILD_DIR)/smp_sync.o: $(EMU_DIR)/smp_sync.c $(EMU_DIR)/smp_sync.h $(EMU_DIR)/smp_program.h $(EMU_DIR)/rv32_system.h $(EMU_DIR)/rv32_cpu.h

.PHONY: all clean install uninstall run debug interactive run-port test-program demo test test-smp start-emu stop-emu gdb-connect help