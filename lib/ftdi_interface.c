#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libftdi1/ftdi.h>

#include "ftdi_interface.h"

struct ftdi_context *ftdi_open (struct ftdi_context *ftdi)
{
   /* Initialises a new ftdi context structure, as well as all usb parameters required for 
   ftdi communication.
   Returns the initialised ftdi structure. */
   int ret;
   struct ftdi_version_info version;

   /* print libftdi info */
   version = ftdi_get_library_version ();
   printf ("INFO: Initialized libftdi %s (major: %d, minor: %d, micro: %d, snapshot ver: %s)\n",
            version.version_str, version.major, version.minor, version.micro, version.snapshot_str);
   
   /* allocate a new ftdi context structure */
   if ((ftdi = ftdi_new ()) == 0)
   {
      fprintf (stderr, "ERROR: failed to initialise ftdi structure\n");
      exit (EXIT_FAILURE);
   }
   
   /* set Interface A */
   if ((ret = ftdi_set_interface (ftdi, INTERFACE_A)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to set interface: %d (%s)\n", ret);
   
   /* open USB connection and connect to ft2232h */
   if ((ret = ftdi_usb_open (ftdi, 0x0403, 0x6010)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to open ftdi device: %d (%s)\n", ret);
   
   /* reset usb parameters */
   if ((ret = ftdi_usb_reset (ftdi)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to reset ftdi device: %d (%s)\n", ret);
   
   /* not needed (yet?) */
   /* set write chunk size */
   /*if ((ret = ftdi_write_data_set_chunksize (ftdi, 65536)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to set write chunk size: %d (%s)\n", ret);
   
   /* set read chunk size */
   /*if ((ret = ftdi_read_data_set_chunksize (ftdi, 65536)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to set read chunk size: %d (%s)\n", ret);
   
   /* set latency timer */
   /*if ((ret = ftdi_set_latency_timer (ftdi, 1)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to set latency timer: %d (%s)\n", ret);
   
   /* AN_114 note */
   usleep (50000);
   
   return ftdi;
}

void ftdi_close (struct ftdi_context *ftdi)
{
   /* Close communication with ftdi device. */
   int ret;
   
   /* close usb connection */
   if ((ret = ftdi_usb_close (ftdi)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to close ftdi device: %d (%s)\n", ret);
   
   ftdi_free (ftdi);
   
   return;
}

void ftdi_exit (struct ftdi_context *ftdi, char *error_string, int error_code)
{
   /* Auxiliary function used to abort program execution if a communication error is received. */
   fprintf (stderr, error_string, error_code, ftdi_get_error_string (ftdi));
   
   ftdi_free (ftdi);
   exit (EXIT_FAILURE);
}

int ftdi_write_data_and_check (struct ftdi_context *ftdi, unsigned char *buf, int size)
{
   /* Write data (command) to ftdi device, then check if ftdi device received a bad command or not.
      Returns 0 on bad command received, < 0 if communication error, > 0 if success (offset). */
   int ret, written_offset;
   unsigned char rxBuf[2];
   
   if ((ret = ftdi_write_data (ftdi, buf, size)) < 0)
      return ret;
   
   /* save written offset */
   written_offset = ret;
   
   /* read two bytes */
   if ((ret = ftdi_read_data (ftdi, rxBuf, 2)) < 0)
      return ret;
   
   /* check if ftdi received an invalid command */
   if (rxBuf[0] == BAD_COMMAND)
   {
      printf ("WARNING: Device received an invalid command: 0x%X\n", rxBuf[1]);
      return 0;
   }
   
   return written_offset;    /* returns written data offset */
}

int ftdi_read_data_and_wait (struct ftdi_context *ftdi, unsigned char *buf, int size)
{
   /* Read data from ftdi device and waits if data is not available yet.
      Returns < 0 if communication error, > 0 if success (read data size). */
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

int ftdi_write_data_and_wait (struct ftdi_context *ftdi, unsigned char *buf, int size)
{
   /* Write data to ftdi device and waits if data cannot be written yet.
      Returns < 0 if communication error, > 0 if success (written data size). */
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
      Returns 1 if empty, 0 if not empty, <0 if cannot poll modem status. */
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
      Returns 1 if no error occured, 0 if an error occured, <0 if cannot poll modem status. */
   int ret;
   union ftdi_status temp_status;

   if ((ret = ftdi_poll_modem_status (ftdi, (unsigned short int *)&temp_status) < 0))
      ftdi_exit (ftdi, "ERROR: Unable to poll modem status: %d (%s)\n", ret);
   
   if (status != NULL)
      *status = temp_status;

   return !(temp_status.OE || temp_status.PE || temp_status.FE);
}