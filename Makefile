CC := i686-elf-gcc 
AR := i686-elf-ar
SYSROOT := sysroot
ISO_DIR := isodir
KERNEL_SRC := kernel-src
LIB_SRC := libc-src
LINKER_FILE := linker.ld
GRUB_FILE := grub.cfg
KERNEL_NAME := bulbOS.kernel
ISO_NAME := bulbOS.iso
ORIG_DISK := disk-original.ext2
COPY_DISK := disk.ext2

WARNINGS := -Wall -Wextra -pedantic
CFLAGS := --sysroot=$(SYSROOT) -isystem=/usr/include -g -std=gnu99 -ffreestanding \
	-fstack-protector -mgeneral-regs-only $(WARNING) 
LIBK_FLAGS := -D__is_libk
QEMU_FLAGS := -d int,out_asm -D file.log --no-reboot 
LIBS := -nostdlib -lgcc -lk

RESET := \033[0m
ECHO_COLOR := \033[1;37m
COMPLETE_COLOR := \033[1;32m

DIRS_INCLUDE := $(patsubst %,%/include/., $(KERNEL_SRC) $(LIB_SRC))

CRTI_FILE := $(shell find $(KERNEL_SRC) -type f -name "crti.S")
CRTN_FILE := $(shell find $(KERNEL_SRC) -type f -name "crtn.S")
KERNEL_FILES := $(shell find $(KERNEL_SRC) \
		 -type f \( -not -name "crt*" \) -and \( -name "*.c" -o -name "*.S" \))
LIB_FILES := $(shell find $(LIB_SRC) -type f -name "*.c" -o -name "*.S")
HEAD_FILES := $(shell find $(PROJDIRS) -type f -name "*.h")

CRTI_OBJ := $(patsubst %.S,%.o, $(CRTI_FILE))
CRTBEGIN_OBJ := $(patsubst %i.S,%begin.o, $(CRTI_FILE))
CRTN_OBJ := $(patsubst %.S,%.o, $(CRTN_FILE))
CRTEND_OBJ := $(patsubst %n.S,%end.o, $(CRTN_FILE))
SRC_OBJS := $(patsubst %.c,%.o, $(patsubst %.S,%.o, $(KERNEL_FILES)))
KERNEL_OBJS := $(CRTI_OBJ) $(CRTBEGIN_OBJ) $(SRC_OBJS) $(CRTEND_OBJ) $(CRTN_OBJ)

LIB_OBJS := $(patsubst %.c,%.o, $(patsubst %.S,%.o, $(LIB_FILES)))
DEP_FILES := $(patsubst %.o,%.d, $(KERNEL_OBJS) $(LIB_OBJS))
LINK_LIST := $(CRTI_OBJ) $(CRTBEGIN_OBJ) $(SRC_OBJS) $(LIBS) $(CRTEND_OBJ) $(CRTN_OBJ)

.PHONY: all clean install_headers libs install_libs install_kernel create_iso call_qemu \
	compile qemu debug

all: compile call_qemu
compile: clean install_headers install_libs install_kernel create_iso
qemu: call_qemu
debug: compile call_qemu_gdb

clean:
	@ rm $(SYSROOT)/* -rf
	@ rm $(ISO_DIR)/* -rf
	@ rm $(ISO_NAME) -f
	@ rm $(KERNEL_NAME) -f
	@ rm $(KERNEL_OBJS) $(LIB_OBJS) $(DEP_FILES) -f
	@ rm libk.a -f 

install_headers:
	@ printf "$(ECHO_COLOR)installing headers...$(RESET)\n"
	@ mkdir $(SYSROOT)/usr/include -p
	@ cp -R --preserve=timestamps $(DIRS_INCLUDE) $(SYSROOT)/usr/include/.
	@ echo "$(COMPLETE_COLOR) complete$(RESET)"

install_libs: libk.a
	@ printf "$(ECHO_COLOR)installing libs...$(RESET)\n"
	@ mkdir $(SYSROOT)/usr/lib
	@ cp -R --preserve=timestamps libk.a $(SYSROOT)/usr/lib/.
	@ echo "$(COMPLETE_COLOR) complete$(RESET)"

install_kernel: $(KERNEL_NAME)
	@ printf "$(ECHO_COLOR)installing kernel...$(RESET)\n"
	@ mkdir $(SYSROOT)/boot -p
	@ cp -R --preserve=timestamps $(KERNEL_NAME) $(SYSROOT)/boot/.
	@ echo "$(COMPLETE_COLOR) complete$(RESET)"

create_iso:
	@ printf "$(ECHO_COLOR)creating ISO...$(RESET)\n"
	@ mkdir $(ISO_DIR)/boot/grub -p
	@ cp $(SYSROOT)/boot/$(KERNEL_NAME) $(ISO_DIR)/boot/$(KERNEL_NAME)
	@ cp $(GRUB_FILE) $(ISO_DIR)/boot/grub/grub.cfg
	@ echo "$(COMPLETE_COLOR) complete$(RESET)"
	@ grub-mkrescue -o $(ISO_NAME) $(ISO_DIR)

call_qemu:
	@ echo "$(ECHO_COLOR)starting qemu...$(RESET)"
	@ cp $(ORIG_DISK) $(COPY_DISK) 
	@ qemu-system-i386 -cdrom $(ISO_NAME) -m 512M \
	-drive file=$(COPY_DISK),format=raw $(QEMU_FLAG) &

call_qemu_gdb:
	@ echo "$(ECHO_COLOR)starting qemu...$(RESET)"
	@ cp $(ORIG_DISK) $(COPY_DISK) 
	@ qemu-system-i386 -cdrom $(ISO_NAME) -m 512M \
	-drive file=$(COPY_DISK),format=raw $(QEMU_FLAG) -s -S &
	@ gdb

libk.a: $(LIB_OBJS)
	@ $(AR) rcs $@ $(LIB_OBJS)

$(KERNEL_NAME): $(KERNEL_OBJS)
	@ $(CC) -T $(LINKER_FILE) -o $@ $(CFLAGS) $(LINK_LIST)
	@ grub-file --is-x86-multiboot $(KERNEL_NAME)

$(CRTBEGIN_OBJ) $(CRTEND_OBJ):
	@ OBJ=`$(CC) $(CFLAGS) -print-file-name=$(@F)` && cp "$$OBJ" $@

%.o: %.c
	@ $(CC) -MMD -c $< -o $@ $(CFLAGS) $(LIBK_FLAGS)

%.o: %.S
	@ $(CC) -MMD -c $< -o $@ $(CFLAGS) $(LIBK_FLAGS)


-include $(DEP_FILES)
