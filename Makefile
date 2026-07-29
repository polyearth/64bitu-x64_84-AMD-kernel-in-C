CC = x86_64-elf-gcc
CFLAGS = -ffreestanding -mcmodel=large -mno-red-zone -mno-mmx -mno-sse -mno-sse2 -O2 -Wall -Wextra

.PHONY: all run clean

all: myos.iso disk.img

# PAPILDINĀTS: Pielikts disk.img kā atkarība, lai Makefile to pārbauda pirms palaišanas
run: myos.iso disk.img
	qemu-system-x86_64 -boot d -cdrom myos.iso -drive file=disk.img,format=raw,index=0,media=disk -no-reboot

# JAUNS: Izveido 10MB disku TIKAI tad, ja datorā tāda vēl nav. Veco disku nekad nedzēš!
disk.img:
	dd if=/dev/zero of=disk.img bs=1M count=10
	mkfs.fat -F 16 disk.img
src/boot.o: src/boot.S
	$(CC) $(CFLAGS) -c src/boot.S -o src/boot.o

src/kernel.o: src/kernel.c
	$(CC) $(CFLAGS) -c src/kernel.c -o src/kernel.o

src/vga.o: src/vga.c
	$(CC) $(CFLAGS) -c src/vga.c -o src/vga.o

src/keyboard.o: src/keyboard.c
	$(CC) $(CFLAGS) -c src/keyboard.c -o src/keyboard.o

src/ata.o: src/ata.c
	$(CC) $(CFLAGS) -c src/ata.c -o src/ata.o

src/editor.o: src/editor.c
	$(CC) $(CFLAGS) -c src/editor.c -o src/editor.o

src/fat.o: src/fat.c
	$(CC) $(CFLAGS) -c src/fat.c -o src/fat.o

src/mouse.o: src/mouse.c
	$(CC) $(CFLAGS) -c src/mouse.c -o src/mouse.o

src/gui.o: src/gui.c
	$(CC) $(CFLAGS) -c src/gui.c -o src/gui.o

src/desktop.o: src/desktop.c
	$(CC) $(CFLAGS) -c src/desktop.c -o src/desktop.o

src/file_manager.o: src/file_manager.c
	$(CC) $(CFLAGS) -c src/file_manager.c -o src/file_manager.o

kernel.elf: src/boot.o src/kernel.o src/vga.o src/keyboard.o src/ata.o src/editor.o src/fat.o src/mouse.o src/gui.o src/desktop.o src/file_manager.o
	$(CC) -ffreestanding -T src/linker.ld -nostdlib -lgcc -Wl,--no-warn-rwx-segments src/boot.o src/kernel.o src/vga.o src/keyboard.o src/ata.o src/editor.o src/fat.o src/mouse.o src/gui.o src/desktop.o src/file_manager.o -o kernel.elf

myos.iso: kernel.elf
	mkdir -p iso_root/boot/grub
	cp kernel.elf iso_root/boot/
	echo 'set timeout=0' > iso_root/boot/grub/grub.cfg
	echo 'set default=0' >> iso_root/boot/grub/grub.cfg
	echo 'menuentry "KristapsOS" {' >> iso_root/boot/grub/grub.cfg
	echo '    multiboot2 /boot/kernel.elf' >> iso_root/boot/grub/grub.cfg
	echo '    boot' >> iso_root/boot/grub/grub.cfg
	echo '}' >> iso_root/boot/grub/grub.cfg
	grub-mkrescue -o myos.iso iso_root

clean:
	rm -f src/*.o src/boot.s *.elf *.iso
	rm -rf iso_root/boot
