# ============================================================
# Bare Metal RTOS Makefile
# ============================================================

# --- Board selection ---
BOARD   ?= esp32c3
BOARDDIR := boards/$(BOARD)

# --- Toolchain ---
ifeq ($(BOARD),qemu-riscv32-virt)
TOOLCHAIN_PATH ?= /opt/riscv/bin
CROSS   ?= $(TOOLCHAIN_PATH)/riscv32-unknown-elf
GDB     ?= $(TOOLCHAIN_PATH)/riscv32-unknown-elf-gdb
QEMU    ?= /opt/riscv/bin/qemu-system-riscv32
QEMU_MACHINE ?= virt
QEMU_CPUS ?= 1
QEMU_MEM ?= 128M
LDLIBS  ?= -lgcc
else ifeq ($(BOARD),qemu-stm32f103c8t8)
TOOLCHAIN_PATH ?= /usr/bin
CROSS   ?= $(TOOLCHAIN_PATH)/arm-none-eabi
GDB     ?= $(TOOLCHAIN_PATH)/arm-none-eabi-gdb
QEMU    ?= qemu-system-arm
QEMU_MACHINE ?= stm32vldiscovery
QEMU_CPUS ?= 1
QEMU_MEM ?=
LDLIBS  ?= -lgcc
else
TOOLCHAIN_PATH ?= $(HOME)/.espressif/tools/riscv32-esp-elf/esp-14.2.0_20251107/riscv32-esp-elf/bin
CROSS   ?= $(TOOLCHAIN_PATH)/riscv32-esp-elf
GDB_TOOLCHAIN_PATH ?= $(HOME)/.espressif/tools/riscv32-esp-elf-gdb/16.3_20250913/riscv32-esp-elf-gdb/bin
GDB     ?= $(GDB_TOOLCHAIN_PATH)/riscv32-esp-elf-gdb
LDLIBS  ?=
endif

CC      := $(CROSS)-gcc
OBJCOPY := $(CROSS)-objcopy
OBJDUMP := $(CROSS)-objdump
SIZE    := $(CROSS)-size
CLANG_FORMAT ?= clang-format

# --- Debug tools ---
OPENOCD_PATH ?= $(HOME)/.espressif/tools/openocd-esp32/v0.12.0-esp32-20251215/openocd-esp32
OPENOCD ?= $(OPENOCD_PATH)/bin/openocd
OPENOCD_SCRIPTS ?= $(OPENOCD_PATH)/share/openocd/scripts
OPENOCD_BOARD ?= board/esp32c3-builtin.cfg
OPENOCD_SPEED ?= 5000
GDB_HOST ?= localhost
GDB_PORT ?= 3333
MONITOR_SECONDS ?= 8

# --- Directories ---
APPDIR  := app
APP     ?= main
KERNELDIR := src/kernel
BUILDDIR := build/$(BOARD)

# --- Target ---
TARGET  := rtos

# --- Sources ---
ifeq ($(APP),main)
APP_SRCS ?= $(APPDIR)/main.c \
            $(APPDIR)/test_timer.c \
            $(APPDIR)/test_sync.c \
            $(APPDIR)/test_isr.c
else
APP_SRCS ?= $(APPDIR)/$(APP).c
endif

SRCS    := $(APP_SRCS) \
           $(wildcard $(KERNELDIR)/*.c) \
           $(wildcard $(BOARDDIR)/*.c)
OBJS    := $(patsubst %.c,$(BUILDDIR)/%.o,$(SRCS))
DEPS    := $(OBJS:.o=.d)
FORMAT_SRCS := $(shell find app boards include src -type f \( -name '*.c' -o -name '*.h' -o -name '*.S' \))

# --- Compiler flags ---
ifeq ($(BOARD),qemu-stm32f103c8t8)
ARCH    := -mcpu=cortex-m3 -mthumb
ARCH_DEFS := -DARCH_ARM_CORTEX_M=1
ARCH_INCLUDES := -Iinclude/arch/arm_cortex_m
else
ARCH    := -march=rv32imc_zicsr -mabi=ilp32
ARCH_DEFS :=
ARCH_INCLUDES := -Iinclude/arch/riscv
endif

CFLAGS  := $(ARCH) \
            $(ARCH_DEFS) \
            -O1 \
            -g \
            -nostdlib \
            -nostartfiles \
            -ffreestanding \
            -Wall \
            -Wextra \
            -I$(BOARDDIR)/include \
            -Iinclude \
            $(ARCH_INCLUDES) \
            -MMD -MP

LDFLAGS := $(ARCH) \
            -nostdlib \
            -T $(BOARDDIR)/ld/link.ld \
            -Wl,--gc-sections \
            -Wl,-Map=$(BUILDDIR)/$(TARGET).map

# --- Flash settings ---
PORT    ?= /dev/ttyACM0
BAUD    ?= 921600
CHIP    := esp32c3

# ============================================================

.PHONY: all clean flash disasm size format openocd openocd-low-speed openocd-sudo gdb gdb-kernel gdb-help monitor-reset qemu FORCE

all: $(BUILDDIR)/$(TARGET).bin

# Create build directory
$(BUILDDIR):
	mkdir -p $@

# Link ELF
$(BUILDDIR)/$(TARGET).elf: $(OBJS) $(BOARDDIR)/ld/link.ld FORCE | $(BUILDDIR)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

# Compile C
$(BUILDDIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# Assemble
$(BUILDDIR)/%.o: %.S
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# Generate direct-boot raw binary. Flash offset follows the LMA and starts at 0x0.
$(BUILDDIR)/$(TARGET).bin: $(BUILDDIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

# Flash to board
flash: $(BUILDDIR)/$(TARGET).bin
	esptool.py --chip $(CHIP) \
	    --port $(PORT) \
	    --baud $(BAUD) \
	    --before usb_reset \
	    write_flash 0x0 $<

# Start OpenOCD in one terminal, then run `make gdb` or `make gdb-kernel`
# from another terminal. ESP32-C3 built-in USB JTAG uses /dev/ttyACM*.
openocd:
	$(OPENOCD) -s $(OPENOCD_SCRIPTS) -f $(OPENOCD_BOARD)

openocd-low-speed:
	$(OPENOCD) -s $(OPENOCD_SCRIPTS) \
	    -f interface/esp_usb_jtag.cfg \
	    -c "adapter speed $(OPENOCD_SPEED)" \
	    -f target/esp32c3.cfg

openocd-sudo:
	sudo $(OPENOCD) -s $(OPENOCD_SCRIPTS) -f $(OPENOCD_BOARD)

# Attach GDB to an already-flashed image and load symbols from rtos.elf.
gdb: $(BUILDDIR)/$(TARGET).elf
	$(GDB) $< \
	    -ex "target extended-remote $(GDB_HOST):$(GDB_PORT)" \
	    -ex "monitor reset halt"

# Common boot debug entry: stop at kernel_main, then let GDB stay interactive.
gdb-kernel: $(BUILDDIR)/$(TARGET).elf
	$(GDB) $< \
	    -ex "target extended-remote $(GDB_HOST):$(GDB_PORT)" \
	    -ex "monitor reset halt" \
	    -ex "break kernel_main" \
	    -ex "continue"

gdb-help:
	@echo "Terminal 1: make openocd"
	@echo "Terminal 2: make gdb"
	@echo "Boot stop:  make gdb-kernel"
	@echo "If OpenOCD reports LIBUSB_ERROR_ACCESS, run: make openocd-sudo"
	@echo "Low speed:  make openocd-low-speed OPENOCD_SPEED=500"
	@echo "Serial:     make monitor-reset"
	@echo "Override:    make openocd OPENOCD_BOARD=board/esp32c3-ftdi.cfg"

monitor-reset:
	python3 tools/serial_reset_read.py --port $(PORT) --seconds $(MONITOR_SECONDS)

ifeq ($(BOARD),qemu-stm32f103c8t8)
qemu: $(BUILDDIR)/$(TARGET).elf
	$(QEMU) -M $(QEMU_MACHINE) -nographic \
	    -semihosting-config enable=on,target=native -kernel $<
else
qemu: $(BUILDDIR)/$(TARGET).elf
	$(QEMU) -machine $(QEMU_MACHINE) -nographic -bios none \
	    -smp $(QEMU_CPUS) -m $(QEMU_MEM) -kernel $<
endif

# Disassemble for debugging
disasm: $(BUILDDIR)/$(TARGET).elf
	$(OBJDUMP) -d -S $< | less

# Show section sizes
size: $(BUILDDIR)/$(TARGET).elf
	$(SIZE) $<

format:
	$(CLANG_FORMAT) -i $(FORMAT_SRCS)

clean:
	rm -rf build

FORCE:

-include $(DEPS)
