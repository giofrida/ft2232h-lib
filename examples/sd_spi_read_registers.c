#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libftdi1\ftdi.h>
#include <ctype.h>

#include "..\lib\ftdi_interface.h"
#include "..\lib\ftdi_spi.h"
#include "..\lib\sd_spi.h"

int main (int argc, char *argv[])
{
   struct ftdi_context *ftdi;
   struct spi_context *spi;
   
   int sd_version;
   dword ocr;
   struct sd_cid cid;
   struct sd_csd csd;

   /* init ftdi communication (usb paramters) */
   ftdi = ftdi_open ();
   
   /* init spi communication: spi mode 1, 0x0014 divider (=400 kHz), divide by 5 on, MSB first */
   spi = spi_init (ftdi, 1, 1, 14, 1, 1, 1, 1, 1, 0);

   /* initialise sd card */
   sd_init (ftdi, spi);
   
   
   spi_open (ftdi, spi);
   
   /* soft reset card */
   sd_reset (ftdi, spi);
      
   /* recognize sd card */
   sd_version = sd_recognize (ftdi, spi); 
   
   printf ("INFO: SD card version: ");
   switch (sd_version)
   {     
      case 0: printf ("MMC Version 3\n");
              break;
      case 1: printf ("SD Version 1\n");
              break;
      case 2: printf ("SD Version 2 (Byte address)\n");
              break;
      case 3: printf ("SD Version 2 (Block address)\n");
              break;
   }
   
   /* get vdd working range */
   if (sd_get_ocr (ftdi, spi, &ocr) > 0)
      sd_print_ocr_info (ocr);               
   
   /* get contents of cid register */
   if (sd_get_cid (ftdi, spi, &cid) > 0)
      sd_print_cid_info (cid);
   
   /* get contents of cid register */
   if (sd_get_csd (ftdi, spi, &csd) > 0)
      sd_print_csd_info (csd);
   
   spi_close (ftdi, spi);
   
   
   ftdi_free (ftdi);
   spi_free (spi);
   return EXIT_SUCCESS;
}