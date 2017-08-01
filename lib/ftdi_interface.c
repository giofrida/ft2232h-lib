#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libftdi1/ftdi.h>

#include "ftdi_interface.h"

struct ftdi_context *ftdi_open (struct ftdi_context *ftdi)
{
   int ret;
   struct ftdi_version_info version;

   // Print libftdi info
   version = ftdi_get_library_version ();
   printf ("INFO: Initialized libftdi %s (major: %d, minor: %d, micro: %d, snapshot ver: %s)\n",
            version.version_str, version.major, version.minor, version.micro, version.snapshot_str);
   
   // Allocate a new FTDI context structure
   if ((ftdi = ftdi_new ()) == 0)
   {
      fprintf (stderr, "ERROR: failed to initialise ftdi structure\n");
      exit (EXIT_FAILURE);
   }
   
   // Set Interface A
   if ((ret = ftdi_set_interface (ftdi, INTERFACE_A)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to set interface: %d (%s)\n", ret);
   
   // Open USB connection and connect to FT2232H
   if ((ret = ftdi_usb_open (ftdi, 0x0403, 0x6010)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to open ftdi device: %d (%s)\n", ret);
   
   // Reset FTDI device    ??? needed?
   if ((ret = ftdi_usb_reset (ftdi)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to reset ftdi device: %d (%s)\n", ret);
   
   /*
   // Set write chunk size
   if ((ret = ftdi_write_data_set_chunksize (ftdi, 65536)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to set write chunk size: %d (%s)\n", ret);
   
   // Set read chunk size
   if ((ret = ftdi_read_data_set_chunksize (ftdi, 65536)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to set read chunk size: %d (%s)\n", ret);
   */
   /*
   // Set latency timer
   if ((ret = ftdi_set_latency_timer (ftdi, 1)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to set latency timer: %d (%s)\n", ret);
   */   
   
   // AN_114 note
   usleep (50000);
   
   return ftdi;
}

void ftdi_close (struct ftdi_context *ftdi)
{
   int ret;
   
   // Close USB connection
   if ((ret = ftdi_usb_close (ftdi)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to close ftdi device: %d (%s)\n", ret);
   
   ftdi_free (ftdi);
   
   return;
}

void ftdi_exit (struct ftdi_context *ftdi, char *error_string, int error_code)
{
   fprintf (stderr, error_string, error_code, ftdi_get_error_string (ftdi));
   
   ftdi_free (ftdi);
   exit (EXIT_FAILURE);
}

int ftdi_write_data_and_check (struct ftdi_context *ftdi, char *buf, int size)
{
   int ret, written_offset;
   unsigned char rxBuf[2];
   
   if ((ret = ftdi_write_data (ftdi, buf, size)) < 0)
      return ret;
   
   // Save written offset
   written_offset = ret;
   
   // Read two bytes
   if ((ret = ftdi_read_data (ftdi, rxBuf, 2)) < 0)
      return ret;
   
   //printf ("-- READ DATA -- : 0x%X 0x%X\n", rxBuf[0], rxBuf[1]);
   
   // Check if FTDI received an invalid command
   if (rxBuf[0] == BAD_COMMAND)
   {
      printf ("WARNING: Device received an invalid command: 0x%X\n", rxBuf[1]);
      return 0;
   }
   
   return written_offset;    // Returns written data offset
}

int ftdi_wait_and_read_data (struct ftdi_context *ftdi, unsigned char *buf, int size)
{
   unsigned int data = size;
   int ret;

   while (data > 0)
   {
      if ((ret = ftdi_read_data (ftdi, buf, data)) < 0)
         return ret;

      buf += ret;
      data -= ret;
   }

   return size;
}

int ftdi_wait_and_write_data (struct ftdi_context *ftdi, unsigned char *buf, int size)
{
   unsigned int data = size;
   int ret;

   while (data > 0)
   {
      if ((ret = ftdi_write_data (ftdi, buf, data)) < 0)
         return ret;

      buf += ret;
      data -= ret;
   }

   return size;
}

int is_tx_buffer_empty (struct ftdi_context *ftdi, union ftdi_status *status)
{
   /* Checks if transmitter buffer is empty.
   The function returns 1 if empty, 0 if not empty, <0 if cannot poll modem status */
   int ret;
   union ftdi_status temp_status;
   
   if ((ret = ftdi_poll_modem_status (ftdi, (unsigned short int *)&temp_status) < 0))
      ftdi_exit (ftdi, "ERROR: Unable to poll modem status: %d (%s)\n", ret);

   if (status != NULL)
      *status = temp_status;

   return temp_status.TEMT;
}

int is_tx_error (struct ftdi_context *ftdi, union ftdi_status *status)
{
   /* Checks if any transmission error occured.
   The function returns 1 if no error occured, 0 if an error occured, <0 if cannot poll modem status */
   int ret;
   union ftdi_status temp_status;

   if ((ret = ftdi_poll_modem_status (ftdi, (unsigned short int *)&temp_status) < 0))
      ftdi_exit (ftdi, "ERROR: Unable to poll modem status: %d (%s)\n", ret);
   
   if (status != NULL)
      *status = temp_status;

   return !(temp_status.OE || temp_status.PE || temp_status.FE);
}