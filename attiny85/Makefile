TARGET := tiny_lamp_attiny85
MCU := attiny85
F_CPU := 8000000UL
PROGRAMMER ?= stk500v1
PORT ?= $(firstword $(wildcard /dev/cu.usbmodem* /dev/ttyACM* /dev/ttyUSB*))
BAUD ?= 19200

CC := avr-gcc
OBJCOPY := avr-objcopy
SIZE := avr-size
AVRDUDE := avrdude

CFLAGS := -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -flto -std=gnu11 \
	-Wall -Wextra -Werror
LDFLAGS := -mmcu=$(MCU) -flto

BUILD_DIR := build
ELF := $(BUILD_DIR)/$(TARGET).elf
HEX := $(BUILD_DIR)/$(TARGET).hex

.PHONY: all clean flash size

all: $(HEX) size

$(BUILD_DIR):
	mkdir -p $@

$(ELF): main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex -R .eeprom $< $@

size: $(ELF)
	$(SIZE) --mcu=$(MCU) --format=avr $<

flash: $(HEX)
	@test -n "$(PORT)" || { echo "No ArduinoISP serial port found" >&2; exit 1; }
	$(AVRDUDE) -p t85 -c $(PROGRAMMER) -P $(PORT) -b $(BAUD) \
		-U flash:w:$(HEX):i

clean:
	rm -rf $(BUILD_DIR)
