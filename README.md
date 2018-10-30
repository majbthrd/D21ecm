USB CDC-ECM example for SAMD21
==============================

Inspired by [lrndis](https://github.com/fetisov/lrndis), this is a CDC-ECM implementation for the Atmel/Microchip SAMD21.  It leverages a variant of the SAMDx1 USB stack from [vcp](https://github.com/ataradov/vcp).

As with [lrndis](https://github.com/fetisov/lrndis), [lwIP](https://savannah.nongnu.org/projects/lwip/) is utilized to provide a rudimentary TCP/IP stack, DHCP server, and web server. 

This is closely related to [D21rndis](https://github.com/majbthrd/D21rndis/), a RNDIS implementation with which it shares most of the same code.

## Usage

Due to the high memory usage of TCP/IP, a SAMD21 with 32kBytes of RAM and at least 128kBytes of FLASH is needed.  Development was done with the [SAMD21 Xplained Pro](https://www.microchip.com/developmenttools/ProductDetails/ATSAMD21-XPRO) (ATSAMD21J18), but the [SparkFun SAMD21 Mini Breakout](https://www.sparkfun.com/products/13664) (ATSAMD21G18) and [Arduino Zero](https://store.arduino.cc/usa/arduino-zero) (ATSAMD21G18) ought to also be feasible.

## Specifics

Look at the ./project/app.c to get an idea of how the code could be modified.  As written, one quantity (systick) is shown in real-time as "Device Time" on the embedded web server (192.168.7.1) and another three quantities (alpha, bravo, and charlie) are "User Controls" on the web page that cause app.c code to be executed.

## Requirements for compiling

[Rowley Crossworks for ARM](http://www.rowley.co.uk/arm/) is presently needed to compile this code.  The project file is under ./ide/Rowley/.

All the code is gcc/clang compatible, and as time permits, other options may be added.
