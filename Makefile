# ============================================================
# Bare Metal RTOS Makefile
# ============================================================

# --- 工具鏈 ---
TOOLCHAIN_PATH := $(HOME)/.espressif/tools/riscv32-esp-elf/esp-14.2.0_20251107/riscv32-esp-elf/bin
CROSS   := $(TOOLCHAIN_PATH)/riscv32-esp-elf
CC      := $(CROSS)-gcc
OBJCOPY := $(CROSS)-objcopy
OBJDUMP := $(CROSS)-objdump
SIZE    := $(CROSS)-size
CLANG_FORMAT ?= clang-format

# --- Debug tools ---
GDB_TOOLCHAIN_PATH ?= $(HOME)/.espressif/tools/riscv32-esp-elf-gdb/16.3_20250913/riscv32-esp-elf-gdb/bin
GDB     ?= $(GDB_TOOLCHAIN_PATH)/riscv32-esp-elf-gdb
OPENOCD_PATH ?= $(HOME)/.espressif/tools/openocd-esp32/v0.12.0-esp32-20251215/openocd-esp32
OPENOCD ?= $(OPENOCD_PATH)/bin/openocd
OPENOCD_SCRIPTS ?= $(OPENOCD_PATH)/share/openocd/scripts
OPENOCD_BOARD ?= board/esp32c3-builtin.cfg
OPENOCD_SPEED ?= 5000
GDB_HOST ?= localhost
GDB_PORT ?= 3333
MONITOR_SECONDS ?= 8

# --- Board selection ---
BOARD   ?= esp32c3
BOARDDIR := boards/$(BOARD)

# --- 目錄 ---
APPDIR  := app
APP     ?= main
KERNELDIR := src/kernel
BUILDDIR := build/$(BOARD)

# --- 目標 ---
TARGET  := rtos

# --- 原始碼 ---
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

# --- 編譯旗標 ---
ARCH    := -march=rv32imc_zicsr -mabi=ilp32
CFLAGS  := $(ARCH) \
            -O1 \
            -g \
            -nostdlib \
            -nostartfiles \
            -ffreestanding \
            -Wall \
            -Wextra \
            -Iinclude \
            -Iinclude/arch/riscv \
            -I$(BOARDDIR)/include \
            -MMD -MP

LDFLAGS := $(ARCH) \
            -nostdlib \
            -T $(BOARDDIR)/ld/link.ld \
            -Wl,--gc-sections \
            -Wl,-Map=$(BUILDDIR)/$(TARGET).map

# --- 燒錄設定 ---
PORT    ?= /dev/ttyACM0
BAUD    ?= 921600
CHIP    := esp32c3

# ============================================================

.PHONY: all clean flash disasm size format openocd openocd-low-speed openocd-sudo gdb gdb-kernel gdb-help monitor-reset

all: $(BUILDDIR)/$(TARGET).bin

# 建立 build 目錄
$(BUILDDIR):
	mkdir -p $@

# 連結 ELF
$(BUILDDIR)/$(TARGET).elf: $(OBJS) $(BOARDDIR)/ld/link.ld | $(BUILDDIR)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

# 編譯 C
$(BUILDDIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# 組譯
$(BUILDDIR)/%.o: %.S
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# 產生 Direct Boot 原始 binary（flash offset = LMA，直接燒到 0x0）
$(BUILDDIR)/$(TARGET).bin: $(BUILDDIR)/$(TARGET).elf
	$(OBJCOPY) -O binary $< $@

# 燒錄到板子
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

# 反組譯（除錯用）
disasm: $(BUILDDIR)/$(TARGET).elf
	$(OBJDUMP) -d -S $< | less

# 顯示各段大小
size: $(BUILDDIR)/$(TARGET).elf
	$(SIZE) $<

format:
	$(CLANG_FORMAT) -i $(FORMAT_SRCS)

clean:
	rm -rf build

-include $(DEPS)
