# 64bitu-x64_84-AMD-kernel-in-C

A custom 64-bit (x86_64) operating system kernel built completely from scratch. It features a custom assembly-to-C hardware bridge, real persistent disk storage (no volatile RAM-only shortcuts), a graphical GUI environment, and a built-in text editor.

Modules
BootLoader & ASM Bridge: Handles the CPU initialization and transitions the system smoothly into 64-bit Long Mode.

VGA Graphics Driver: A custom UI engine that draws windows, rectangles, text layouts (print_at), and renders a real-time mouse cursor.

Interrupt Handlers (IDT): The low-level Interrupt Descriptor Table bridge connecting hardware interrupts directly to C code.

Keyboard & Mouse Drivers: Captures real-time hardware inputs for smooth typing and mouse tracking.

ATA Hard Drive Driver: Communicates directly with hard drive ports to read and write raw disk sectors.

FAT16 File System: A proper file system implementation supporting subfolder path parsing (e.g., FOLDER/FILE.TXT) with true data persistence across reboots.

Notepad Application: The core built-in text editor application allowing you to write text, load existing files from the drive, and save them persistently.

Requirements
Linux (or WSL)

x86_64-elf-gcc (Cross-compiler toolchain)

NASM (Netwide Assembler)

QEMU (Emulator)

Build
To install the necessary build tools on your Linux system and compile the project:

Bash
sudo apt-get install build-essential nasm qemu-system-x86
# Navigate to your project directory and compile
make
Usage
To compile the bootloader, kernel files, link them into the final binary, and launch the OS inside the QEMU emulator, simply run:

Bash
make run


<img width="742" height="475" alt="image" src="https://github.com/user-attachments/assets/801b2f23-0a3e-4728-894f-1b43bd730d49" />
<img width="731" height="450" alt="image" src="https://github.com/user-attachments/assets/2feb3357-616d-4065-b36a-3be410ab6f32" />
<img width="737" height="469" alt="image" src="https://github.com/user-attachments/assets/b232e8dc-b5ce-4e3f-823d-98caacb12d0c" />
<img width="723" height="468" alt="image" src="https://github.com/user-attachments/assets/223844a9-f31f-47ac-ad0c-6ea25a61187c" />
<img width="729" height="489" alt="image" src="https://github.com/user-attachments/assets/3e8625ca-2bc7-4cf2-a42e-eb7b3c32abe9" />






References
OSDev Wiki (Operating System Development Wiki)

Inline assembly and toolchain documentation for x86_64-elf
