DEVICE = stm32f103c8t6
OPENCM3_DIR = libopencm3
BUILD_DIR = build

SOURCES = src/stm32f1_hc_sr04.c
OBJS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
BIN = binary
BIN_ELF = $(BUILD_DIR)/$(BIN).elf
BIN_BIN = $(BUILD_DIR)/$(BIN).bin
BIN_HEX = $(BUILD_DIR)/$(BIN).hex

ENABLE_ITM ?= 0

CFLAGS += -Wall -g -DENABLE_ITM=$(ENABLE_ITM)
# CFLAGS += -Os -ggdb3
# CPPFLAGS += -MD
LDFLAGS += -static -nostartfiles -specs=nosys.specs -specs=nano.specs -u_printf_float
LDLIBS += -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group

include $(OPENCM3_DIR)/mk/genlink-config.mk
include $(OPENCM3_DIR)/mk/gcc-config.mk

.PHONY: clean all flash

all: $(BIN_BIN) $(BIN_ELF) $(BIN_HEX)

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(BUILD_DIR)
	@printf "  CC      $<\n"
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) $(ARCH_FLAGS) -o $@ -c $<

clean:
	$(Q)$(RM) -rfv $(BUILD_DIR)

flash: $(BIN_BIN)
	@openocd \
		-f interface/stlink.cfg \
		-f target/stm32f1x.cfg \
		-c "program $(BIN_ELF) verify reset exit"

erase:
	$(Q)st-flash erase

gdb:
	@arm-none-eabi-gdb -tui \
		-ex "target extended-remote localhost:3333" \
		-ex "monitor reset halt" \
		-ex "monitor arm semihosting enable"

include $(OPENCM3_DIR)/mk/genlink-rules.mk
include $(OPENCM3_DIR)/mk/gcc-rules.mk
