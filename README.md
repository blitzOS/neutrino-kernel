### ![Neutrino Kernel Project](./utils/neutrino-logo-v2.png)
*Neutrino* is a project that aims to understand and implement a 64 bit kernel written in modern C and Assembly. The kernel will have basic drivers and functionality, but other features may be added in the future. The currently implemented features are listed [here](#Features). 

## Milestones
- [x] 0 ー Reaching Userspace  
    *Implement everything needed for reaching a simple userspace*
- [ ] 1 ー **Storage land** ⬅  
    *Implement a VFS to access installed drives, writing and reading files*
- [ ] 2 ー Network stack   
    *Add a network stack to ping, send and receive packets*
- [ ] 3 ー Graphical impovements   
    *Improve the GUI and the UX of the userspace, adding controls and a window manager*

## Features
- [x] ⛱ **Limine bootloader** as the main boot-up solution for the moment
    - [x] **Stivale2 protocol compliant**   
        *The kernel asks the bootloader for a linear framebuffer, but can also use a terminal if the framebuffer is not available. The bootloader also pass some informations to the kernel.*

- [ ] 🌳 **Basic kernel**
    - [x] **Kernel Services**  
        *The kernel services functions are used to provide a easier to way to interface the kernel and the drivers. These functions will NOT be available to the user space. Also enables kernel components to log and throw fatal exceptions with [specific codes](/utils/documentation/KernelService%20Fatal%20Codes.md)*
    - [x] **GDT setup**   
        *Null descriptor; 32 bit code and data descriptors; 64 bit code and data descriptors. More descriptors will be added when required.*
    - [x] **IDT setup**   
        *Basic x86_64 interrupts and exceptions handled. Software interrupts will be added when necessary.*
    - [x] **Serial comms**   
        *Serial communication with write and read operations from and to a given serial port*
    - [x] **SSE**   
        *Floating point unit initialization and XSAVE/AVX check and enabling*
    - [x] **ACPI**   
        *Basic ACPI tables listing and parsing*
         - [x] **RSDT/XSDT**   
            *with the possibility to find tables*
         - [x] **MADT**   
            *including the LAPIC, IOAPIC, IOAPIC_ISO sub-tables*
    - [x] **APIC**
         - [x] **IOAPIC initialization**   
            *with regards to the Interrupt Source Override table*
         - [x] **LAPIC initialization**
    - [x] **SMD (Symmetric multi-processing)**
        - [x] **Per-CPU initialization (via stivale2)**
    - [x] **CPUID** 
    - [x] **Timers**
        - [x] **RTC** `🚧 WIP`
        - [x] **LAPIC timer**
        - [ ] **PIT** `🔻 LOW PRIORITY`
        - [x] **HPET**
    - [x] **Memory manager**
        - [x] **Physical memory manager**   
            *Scans the loaded memory and manages it using 4KB blocks. Kernel and other reserved areas are marked accordingly*
        - [x] **Virtual memory manager**   
            *Manages the virtual memory page tables. Can map, remap and unmap pages*
        - [x] **Kernel Heap manager**
    - [x] **Executable loading**
    - [x] **Process scheduler** `🔗 Timers, Executable loading`
    - [x] **Virtual Filesystem (VFS)**
    - [x] **IPC**

- [ ] ⚙ **Simple drivers**
    - [ ] **PCI**
        - [x] **Bus scanning and enumeration**
        - [ ] **Device initialization**
    - [ ] **PS/2 Keyboard**

- [ ] ⚙ **Advanced drivers**
    - [x] **Video driver (BGA)**
        - [x] **Plot pixel**
        - [ ] **Draw line**
        - [ ] **Draw complex shapes**
    - [ ] **Network driver**
        - [ ] **Ethernet1000**
        - [ ] **Stack TCP/IP** 

- [ ] 📀 **Storage**
    - [ ] **Drives drivers**
        - [ ] **USB**
        - [ ] **AHCI**
        - [ ] **NVMe**
    - [ ] **Filesystem drivers**
        - [x] **initrd**
        - [ ] **ext2**
        - [ ] **FAT12**

- [ ] 👤 **Userspace**
    - [x] **System calls**
        - [x] `neutrino_log()` log a message to serial output
        - [x] `neutrino_now()` get the current timestamp
        - [x] `neutrino_alloc()` allocate a task heap area
        - [x] `neutrino_free()` release a task heap area
        - [x] `neutrino_ipc()` allows different processes to exchange messages
    - [ ] *And many apps*

## How to
### Build Neutrino
WIP

### Build executables for Neutrino
>⚠ **Only supported format is ELF64**
- Create a folder named `executables` and put your source files in there
- While writing a program, you can only use some of the Neutrino-provided libraries. No standard libraries are supported
- Run the `make executables` command: every source file in the `executables` folder will be compiled as well as the user libraries. 
- To test out your executables, you can (as for now) put them in the `initrd` folder

### Make Initrd
- Create a folder named `initrd` in the project root folder
- Put inside the newly created folder the files you want to load. As for now only custom ELF binaries are supported in execution. Other files will simply be displayed
- Run the `make initrd` command: it will generate a file called `initrd.img`   
_Note that if you compile the main kernel with `make`, the initrd will be created as well and it will copied to the image_

## Source tree
- **`kernel\`**
    - **`x86-64\`** _platform-specific code for the x86-64 architecture_
        - **`device\`** _platform-specific code for device libraries_
            - **`time\`** _source files for time related components (hpet, lapic, etc.)_
        - **`memory\`** _platform-specific code for memory related functions_
        - **`tasks\`** _platform-specific code for tasks related functions_
    - **`common\`** _platform-independent kernel code_
        - **`device\`** _libraries for device abstraction in the kernel_
        - **`fs\`** _filesystem abstraction in the kernel_
        - **`memory\`** _libraries for memory related functions_
        - **`tasks\`** _scheduler, tasks and context related functions_
        - **`video\`** _implementation of video driver_
- **`libs\`**
    - **`liballoc\`** _Durand's Amazing Super Duper Memory allocator_
    - **`libc\`** _porting of useful C libraries_
    - **`limine\`** _limine bootloader's headers and libraries_
    - **`linkedlist\`** _linkedlist implementation_
    - **`neutrino\`** _kernel libraries_
- **`limine\`** [_limine bootloader binaries_](https://github.com/limine-bootloader/limine/tree/v2.0-branch-binary)
- **`utils\`** _file stored as backup or utility_
