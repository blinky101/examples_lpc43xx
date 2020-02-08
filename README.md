# LPC43xx example projects

This repository contains some examples for the LPC43xx microcontroller. These projects are based on the blinky project covered by [this blinky tutorial](https://blinky101.github.io/blinky_lpc43xx/). The project folders also have a readme file.

For flashing you will need either:

* [Black magic probe](https://github.com/blacksphere/blackmagic) You can also turn a cheap bluepill board into a blackmagic probe if you already have another SWD programmer.
* FTDI based jtag/swd programmer such as [JTAG-lock-pick Tiny 2](http://www.distortec.com/jtag-lock-pick-tiny-2/#more-13)

To build these examples you need the following software:

* CMake
* GNU Embedded Toolchain `7-2018-q2-update` (arm-none-eabi-gcc)
* Git
* OpenOCD (if you don't use the Blackmagic probe)




## Before building

Copy the `config.cmake.example` file as `config.cmake` from the repository root to the project folder you want to build. Uncomment and choose your desired optimization level (`set(OPTIMIZE 0)` for debugging). And enter the correct device file for your blackmagic probe.

## Build the firmware the first time

Go into the project root (e.g. `multibllinky`) and create a directory 'build' and go into that directory

```bash
cd multiblinky
mkdir build
cd build
```

Then run cmake to fetch all dependencies and create the makefile. When that has successfully completed we can build the firmware by calling the Makefile.

```bash
cmake ..
make
```

If everything goes smoothly the tail of the output should look like this:

```
Scanning dependencies of target Multiblinky
[ 96%] Building C object CMakeFiles/Multiblinky.dir/src/board.c.obj
[ 98%] Building C object CMakeFiles/Multiblinky.dir/src/main.c.obj
[100%] Linking C executable Multiblinky
[100%] Built target Multiblinky
Scanning dependencies of target bin
[100%] Built target bin
```

and you should find a `elf` file and `.bin` file in your build directory. Ready to flash to the target microcontroller.

Run `make flash` to flash using the [built in tools](https://github.com/JitterCompany/mcu_debug), or use your own method.



