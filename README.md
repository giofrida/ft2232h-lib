# ft2232h-lib
This is a simple set of functions written in C that uses the known [libftdi](https://www.intra2net.com/en/developer/libftdi/) library to communicate with FTDI FT2232H.
<br>The aim of the project is to make programs for this platform much easier and simpler to write. Please take a look at the examples.
<br>Also, note that this is a work in progress and there could be a few bugs.

## Setup ##
#### Windows (Cygwin) ####
1. Install Cygwin along with the following packages
- Devel/gcc-core
- Libs/libftdi1-devel
2. Add (disk):\cygwin(64)\bin folder to the PATH environment variable
3. Download Zadig (http://zadig.akeo.ie/) and plug in your FTDI platform. 
4. If you cannot see your FTDI device, from Zadig choose Options->List all devices. 
5. Select your FTDI device and install libusbK driver. Repeat this step for all the interfaces of your FTDI device.

#### Linux ####
(soon)

## Library ##
My library is divided in different files. At the moment it includes the following
- ftdi_interface: includes initialisation and common functions
- ftdi_spi: includes all the required functions to use the SPI interface on your FTDI device
<br>

- sd_spi: it is a library used by sd_spi_* example(s), created because communication with an SD card cannot be easily done, as it requires many initialisation routines and checks

## Compiling ##
When using gcc you only have to specify the ```.c``` files you are using from my library.
<br>This is a compile example:
<br>```gcc ftdi_test.c -o ftdi_test.exe lib\ftdi_interface.c lib\ftdi_spi.c -llibftdi1```

## Documentation ##
The library has been fast documented using Doxygen. You can read the [documentation here](giofrida.github.io/ft2232h-lib/index.html).
