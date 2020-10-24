# `bt-support`

Demo project mostly geared towards Bluetooth LE
functionality on the Adafruit nRF52840 Feather Express
board. This supports Nordic UART (accessible through the
Bluefruit Connect app) and serial USB UART for piping
`printk()` messages to a serial monitor. An STLink is used
 to flash and debug the board.

The code in this repository is designed to work with an
STLink for debugging and flashing Zephyr. Other debuggers
can be used, such as Raspberry Pi and SEGGER JLink, but
the kernel settings (found in `prj.conf` file in this
project) and the setup instructions in this `README` are
written specifically to work with an STLink.

# Hardware Setup

  * Adafruit nRF52840 Feather Express
  * STLink v2
  * 10-pin 2x5 Socket-Socket 1.27mm IDC (SWD) Cable, broken
  out
  * 4x 2.54 pitch (standard) female-female connectors
  * Micro USB **with data transport**
    * Some USBs are power-only, so verify that the USB can
    carry data
  
SWD header pinout on the nRF52840:

```
           0    1
           2    3
notch-side 4    5
           6    7
           8    9
```

The relevant pins for the STLink are:

  * 0 = 3.3V
  * 1 = SWIO
  * 2 = SWCLK
  * 3 = GND

The STLink should be wired with the 4x female-female
connectors to the broken-out SWD cable in order to gain
debugging capability. The Micro USB should be plugged in
and opened on a serial monitor (such as `minicom`) even if
the board is debugged via the STLink because `printk()`
output is not piped through the debugger.

# Software Setup

On the software-side, the following are needed to work with
the nRF52840 board.

  * [Bluefruit Connect](https://learn.adafruit.com/bluefruit-le-connect/ios-setup) -
  allows access to the Bluetooth LE UART console on a phone
  * `minicom` - command line serial monitor, allows access
  to `printk()` output using a USB
    * Alternatively [MobaXTerm](https://mobaxterm.mobatek.net/)
    can be used on Windows
  * [Zephyr](https://www.zephyrproject.org/) - the
  operating system and build framework used to run the
  application
  * [OpenOCD](http://openocd.org/) - used to debug and
  flash binaries onto the board
  * [CLion](https://www.jetbrains.com/clion/) - CLion is
  was used to develop this application, but any C++ IDE
  should work

### Zephyr Setup

The Zephyr setup can be found [here](https://docs.zephyrproject.org/latest/getting_started/index.html).

The toolchain used to build this project was GNU ARM
Embedded. This is used to compile the code into a binary.
See [this](https://docs.zephyrproject.org/latest/getting_started/toolchain_3rd_party_x_compilers.html#gnu-arm-embedded)
link to install a toolchain.

Zephyr is extremely fickle when it comes to installation
so if you run into build or flash issues, chances are it is
because you either didn't set it up correctly or you have
forgotten to set a few environment variables.

The 3 environment variables needed to build and run are the
`ZEPHYR_TOOLCHAIN_VARIANT`, `GNUARMEMB_TOOLCHAIN_PATH` (if
you are using GNU ARM Embedded) and `ZEPHYR_BASE`.

### OpenOCD Setup for STLink

This project **requires** `OpenOCD`. `PyOCD` WILL NOT WORK.
At the time of writing, you **CANNOT** download an OpenOCD
package to use the STLink. You must build the latest
version of OpenOCD from their git repository in order to
work with an STLink+nRF52840.

The project contains an `openocd.cfg` template that should
be used to manually flash or debug the board.

# Building and Flashing

The `board.cmake` for the `adafruit_feather_nrf52840` by
default does not contain an entry for OpenOCD. This should
be modified because it makes flashing significantly easier.
However, `west build` can be used manually and flashed
using the `openocd` command directly if desired.

```
# Current directory is the project root dir

# Use CMake for project setup
mkdir cmake-build-debug
cd cmake-build-debug && cmake .. && cd ..

# Build and flash
west flash -d cmake-build-debug

# Alternatively flash manually with OpenOCD
# west build -d cmake-build-debug
# openocd -f openocd.cfg -c "init; reset init; flash write_image erase cmake-build-debug/zephyr/zephyr.elf; verify_image cmake-build-debug/zephyr/zephyr.elf; reset; exit"
```

# Debugging

Debugging works by starting a local server that interfaces
between the STLink and debugger software. The server is
started with OpenOCD in the following manner:

```
# Current directory is the project root dir
openocd -f openocd.cfg
```

Then, in a separate terminal window, GDB can be used to
connect to the server and debug the board. Alternatively,
a Remote GDB session can be started in CLion to use a
graphical user interface instead.

```
# Current directory is the project root dir
arm-none-eabi-gdb cmake-build-debug/zephyr/zephyr.elf
```

# Troubleshooting

Generally, issues with board connectivity can be diagnosed
by using the `-d3` flag to print extra information when
invoked. For example: `openocd -f openocd.cfg -d3`.

Common error codes from the STLink:

  * `0x09` - Cannot connect to the target
    * Check the SWD connector, the connectors and the
    wiring; basically the board isn't connected to the
    STLink
  * `0x29` - Board not supported by OpenOCD
    * Compile the most recent version of OpenOCD which
    bundles the nrf5 utility in order to get the STLink to
    work with the board

# Credits

Built with [CLion](https://www.jetbrains.com/clion/)

Utilizes:

  * [Zephyr](https://www.zephyrproject.org/)
