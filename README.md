# TinyRT

TinyRT is a small bare-metal RTOS experiment currently targeting ESP32-C3. It includes basic kernel primitives such as tasks, scheduling, timers, semaphores, mutexes, wait queues, and message queues.

This project boots directly from flash at offset `0x0` on ESP32-C3 and does not use the normal ESP-IDF bootloader flow.

## Basic Usage

Build the default app for ESP32-C3:

```sh
make BOARD=esp32c3
```

Build a specific app:

```sh
make BOARD=esp32c3 APP=msg_queue_test
```

Flash to an ESP32-C3 board:

```sh
make flash BOARD=esp32c3 APP=msg_queue_test PORT=/dev/ttyACM0
```

Reset and read serial output:

```sh
make monitor-reset BOARD=esp32c3 MONITOR_SECONDS=8
```

Format source files:

```sh
make format
```

Remove build artifacts:

```sh
make clean
```
