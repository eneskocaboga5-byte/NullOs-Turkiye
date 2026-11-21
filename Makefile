CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
OBJSIZE = arm-none-eabi-size
ELF2UF2 = tools/elf2uf2.py
INSERT_CHECKSUM_INTO_ELF = tools/checksum_pico_elf.py

BUILD_DIR = build
$(shell mkdir -p $(BUILD_DIR))
$(shell python3 tools/gen_kconfig.py)
include $(BUILD_DIR)/kconfig.mk

INCLUDES = -I$(BUILD_DIR) -Isrc -Idrivers -Ibaremetal/include
VPATH = src:src/kernel:src/drivers:src/subsys:drivers:baremetal
CFLAGS = -ffreestanding -nostartfiles -nodefaultlibs -ffunction-sections -fdata-sections -Wall -mcpu=cortex-m0 -mthumb -mfloat-abi=soft -Os -flto "-DSYS_CLOCK=125000000L" $(INCLUDES)
CFLAGSS = -ffreestanding -nostartfiles -ffunction-sections -fdata-sections -Wall -mcpu=cortex-m0 -mthumb -mfloat-abi=soft -Os $(INCLUDES)
LINKSCRIPT = baremetal/copy_to_ram.ld

OBJS = $(addprefix $(BUILD_DIR)/,$(NULLOS_OBJS)) $(BUILD_DIR)/usbcdc.o $(BUILD_DIR)/crt0.o $(BUILD_DIR)/_copy_to_ram_stage2_boot.o

all: $(BUILD_DIR)/nullos.uf2

clean:
	rm -rf $(BUILD_DIR) *.o *.out *.elf *.uf2

$(BUILD_DIR)/%_stage2_boot.o: %_stage2_boot.c
	$(CC) $(CFLAGSS) -c $< -o $@

$(BUILD_DIR)/crt0.o: crt0.c
	$(CC) $(CFLAGSS) -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/nullos.out: $(OBJS)
	$(CC) $(CFLAGS) -T $(LINKSCRIPT) -nostdlib -o $@ $^
	@$(OBJSIZE) $@

$(BUILD_DIR)/nullos.elf: $(BUILD_DIR)/nullos.out
	$(OBJCOPY) -O elf32-littlearm $< $@
	$(INSERT_CHECKSUM_INTO_ELF) $@

$(BUILD_DIR)/nullos.uf2: $(BUILD_DIR)/nullos.elf
	$(ELF2UF2) -v $<
