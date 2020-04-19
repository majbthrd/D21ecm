USB CDC-ECM example for SAMD21
==============================

Inspired by [lrndis](https://github.com/fetisov/lrndis), this is a CDC-ECM implementation for the Atmel/Microchip SAMD21.  It leverages a variant of the SAMDx1 USB stack from [vcp](https://github.com/ataradov/vcp).

As with [lrndis](https://github.com/fetisov/lrndis), [lwIP](https://savannah.nongnu.org/projects/lwip/) is utilized to provide a rudimentary TCP/IP stack, DHCP server, and web server. 

This is closely related to [D21rndis](https://github.com/majbthrd/D21rndis/), a RNDIS implementation, and [D21eem](https://github.com/majbthrd/D21eem/), a CDC-EEM implementation.  All implementations share most of the same code, differing only in the driver.

Note that this project was tested with Linux, macOS (High Sierra 10.13.16), and some smartphones (Moto E 2nd Gen and Moto G 1st Gen).

## Alternatives

I have been making pull requests to [TinyUSB](https://github.com/hathach/tinyusb) to provide similar or greater capabilities.  I am hoping this will be a worthwhile expenditure of effort, as [TinyUSB](https://github.com/hathach/tinyusb) would have the added benefit of supporting multiple processors and being build-able with only open source tools.

## Usage

Due to the high memory usage of TCP/IP, a SAMD21 with 32kBytes of RAM and at least 128kBytes of FLASH is needed.  Development was done with the [SAMD21 Xplained Pro](https://www.microchip.com/developmenttools/ProductDetails/ATSAMD21-XPRO) (ATSAMD21J18), but the [SparkFun SAMD21 Mini Breakout](https://www.sparkfun.com/products/13664) (ATSAMD21G18) and [Arduino Zero](https://store.arduino.cc/usa/arduino-zero) (ATSAMD21G18) ought to also be feasible.

## Specifics

Look at the ./project/app.c to get an idea of how the code could be modified.  As written, one quantity (systick) is shown in real-time as "Device Time" on the embedded web server (192.168.7.1) and another three quantities (alpha, bravo, and charlie) are "User Controls" on the web page that cause app.c code to be executed.

## Arduino Zero Boards with Problems

This code normally uses the USBCRM mode of the SAMD21.  In this mode, the part disciplines its own 48MHz RC clock using the USB SOF messages.  The advantage of this is simple: it doesn't require optional external components.
 
Until buying a PCB from Sparkfun, I have had zero issues with USBCRM across several hardware designs.  However, the Sparkfun design (derived from Arduino Zero) uses a 32k external crystal and USB will NOT work reliably on it unless this optional crystal is used as the timing source.  Furthermore, the Arduino Zero bootloader source code depends on a 32k external crystal without any explanation as to why.  There is no verbiage in the datasheet to indicate special requirements for USBCRM.  AT07175 (the SAM-BA app note that served as a basis for the "Arduino Zero" bootloader) makes no special requirements.  There are no errata notes on possible scenarios where USBCRM should not function.

The USBCRM mode should be universal, as it doesn't depend on optional external components.  For that reason, the source code uses this mode by default.  However, the source code now has an "#if 1" that can be changed to "#if 0" to cause the optional external 32k crystal (if populated on your design) to be used instead of USBCRM.

## Requirements for compiling

[Rowley Crossworks for ARM](http://www.rowley.co.uk/arm/) is presently needed to compile this code.  The project file is under ./ide/Rowley/.

All the code is gcc/clang compatible, and as time permits, other options may be added.
