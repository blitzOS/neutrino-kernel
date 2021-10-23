.DEFAULT_GOAL=$(ELF_TARGET)

ARCH 			:= x86_64
BUILD_OUT 		:= ./build
ISO_OUT 		:= ./iso
ELF_TARGET 		:= neutrino.sys
ISO_TARGET 		:= neutrino.iso
DIRECTORY_GUARD  = mkdir -p $(@D)
LD_SCRIPT 		:= $(ARCH).ld

END_PATH 		:= kernel/common kernel/${ARCH} libs/

# compiler and linker
CC 				:= $(ARCH)-elf-gcc
CCPP			:= $(ARCH)-elf-g++

LD 				:= $(ARCH)-elf-ld

# flags
DEFINEFLAGS  	:= -D__$(ARCH:_=-)

INCLUDEFLAGS 	:= -I. \
					-I./kernel/common \
					-I./kernel/$(ARCH) \
					-I./thirdparty/ \
        			-I./libs/libc \
       				-I./libs/ 
					
CFLAGS 			:= 	-g -Wall -Wl,-Wunknown-pragmas -ffreestanding -fpie -fpic -fno-stack-protector -fno-rtti \
        			-fno-exceptions -mno-red-zone -mno-3dnow -MMD -mno-80387 -mno-mmx -mno-sse -mno-sse2 \
					-O2 -pipe $(INCLUDEFLAGS) $(DEFINEFLAGS)

LDFLAGS 		:= 	-T $(LD_SCRIPT) -nostdlib -zmax-page-size=0x1000 -shared -pie -pic \
					--no-dynamic-linker -ztext

# sources and objects
CFILES 			:= $(shell find $(END_PATH) -type f -name '*.c')
CHEADS 			:= $(shell find $(END_PATH) -type f -name '*.h')
COBJ 			:= $(patsubst %.c,$(BUILD_OUT)/%.o,$(CFILES))
HEADDEPS 		:= $(CFILES:.c=.d)

CPPFILES 		:= $(shell find $(END_PATH) -type f -name '*.cpp')
CPPOBJ 			:= $(patsubst %.cpp,$(BUILD_OUT)/%.o,$(CPPFILES))

ASMFILES		:= $(shell find $(END_PATH) -type f -name '*.asm') 
SFILES 			:= $(shell find $(END_PATH) -type f -name '*.s')
ASMOBJ			:= $(patsubst %.asm,$(BUILD_OUT)/%.o,$(ASMFILES)) 
SOBJ			:= $(patsubst %.s,$(BUILD_OUT)/%.o,$(SFILES))
				
CRTI_OBJ		:= $(BUILD_OUT)/kernel/$(ARCH)/support/crti.o
CRTBEGIN_OBJ	:= $(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJ		:= $(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
CRTN_OBJ 		:= $(BUILD_OUT)/kernel/$(ARCH)/support/crtn.o

OBJ 			:= $(shell find $(BUILD_OUT) -type f -name '*.o')
OBJ_LINK_LIST   := $(CRTI_OBJ) $(CRTBEGIN_OBJ) $(COBJ) $(CPPOBJ) $(ASMOBJ) $(CRTEND_OBJ) $(CRTN_OBJ)

# qemu settings
QEMU 			= /mnt/d/Programmi/qemu/qemu-system-${ARCH}.exe
EMU_MEMORY 		= 1G
RUN_FLAGS 		= -m ${EMU_MEMORY} -vga std
DEBUG_FLAGS		= ${RUN_FLAGS} -serial stdio -d guest_errors

# === COMMANDS AND BUILD ========================

-include $(HEADDEPS)
$(BUILD_OUT)/%.o: %.c
	@$(DIRECTORY_GUARD)
	@echo "[KERNEL $(ARCH)] (c) $<"
	@${CC} ${CFLAGS} -c $< -o $@

$(BUILD_OUT)/%.o: %.cpp
	@$(DIRECTORY_GUARD)
	@echo "[KERNEL $(ARCH)] (c++) $<"
	@${CCPP} ${CFLAGS} -c $< -o $@

$(BUILD_OUT)/%.o: %.s
	@$(DIRECTORY_GUARD)
	@echo "[KERNEL $(ARCH)] (asm) $<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_OUT)/%.o: %.asm
	@$(DIRECTORY_GUARD)
	@echo "[KERNEL $(ARCH)] (asm) $<"
	@nasm $< -f elf -o $@

$(ELF_TARGET): $(BUILD_OUT)/$(ELF_TARGET)

.PHONY:$(BUILD_OUT)/$(ELF_TARGET)
$(BUILD_OUT)/$(ELF_TARGET): $(CRTI_OBJ) $(CRTBEGIN_OBJ) $(COBJ) $(CPPOBJ) $(ASMOBJ) $(CRTEND_OBJ) $(CRTN_OBJ)
	@echo "[KERNEL $(ARCH)] (ld) $^"
	@${LD} $^ $(LDFLAGS) -o $@

run: $(ISO_TARGET)
	@rm $(BUILD_OUT)/$(ELF_TARGET)
	@${QEMU} -cdrom $< ${RUN_FLAGS}

debug: $(ISO_TARGET)
	@rm $(BUILD_OUT)/$(ELF_TARGET)
	@${QEMU} -cdrom	$< ${DEBUG_FLAGS} 

clear:
	@echo "[KERNEL $(ARCH)] Removing files..."
	@rm -f $(COBJ) $(CPPOBJ) $(ASMOBJ) $(SOBJ) $(CRTN_OBJ) $(CRTI_OBJ) \
	$(HEADDEPS) $(BUILD_OUT)/$(ELF_TARGET) $(ISO_TARGET)

cd:	$(ISO_TARGET)
	@echo "[KERNEL $(ARCH)] Preparing iso..."

.PHONY:$(ISO_TARGET)
$(ISO_TARGET): $(ELF_TARGET)
	@mkdir -p iso
	@cp -v $(BUILD_OUT)/$< utils/limine.cfg limine/limine.sys \
    	limine/limine-cd.bin limine/limine-eltorito-efi.bin $(ISO_OUT)
	@xorriso -as mkisofs -b limine-cd.bin \
        -no-emul-boot -boot-load-size 4 -boot-info-table \
        --efi-boot limine-eltorito-efi.bin \
        -efi-boot-part --efi-boot-image --protective-msdos-label \
        $(ISO_OUT) -o $@
	@./limine/limine-install $@