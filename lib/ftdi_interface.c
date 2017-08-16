#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <libftdi1/ftdi.h>

#include "ftdi_interface.h"

/**
   Initialises a new ftdi_context structure and configures all the parameters required for 
   a correct USB communication.
   
   @return pointer to initialised ftdi_context
*/
struct ftdi_context *ftdi_open (void)
{
   int ret;
   struct ftdi_context *ftdi;
   struct ftdi_version_info version;

   /* print libftdi info */
   version = ftdi_get_library_version ();
   printf ("INFO: Initialized libftdi %s (major: %d, minor: %d, micro: %d, snapshot ver: %s)\n",
            version.version_str, version.major, version.minor, version.micro, version.snapshot_str);
   
   /* allocate a new ftdi context structure */
   if ((ftdi = ftdi_new ()) == NULL)
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
      ftdi_exit (ftdi, "ERROR: Unable to set write chunk size: %d (%s)\n", ret);*/
   
   /* set read chunk size */
   /*if ((ret = ftdi_read_data_set_chunksize (ftdi, 65536)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to set read chunk size: %d (%s)\n", ret);*/
   
   /* set latency timer: this allows us to retrieve data from device faster */
   if ((ret = ftdi_set_latency_timer (ftdi, 1)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to set latency timer: %d (%s)\n", ret);
   
   /* AN_114 note */
   usleep (50000);
   
   return ftdi;
}

/**
   Closes USB communication with FTDI device and de-allocates ftdi_context structure.
   
   @param ftdi pointer to struct ftdi_context
*/
void ftdi_close (struct ftdi_context *ftdi)
{
   int ret;
   
   /* close usb connection */
   if ((ret = ftdi_usb_close (ftdi)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to close ftdi device: %d (%s)\n", ret);
   
   ftdi_free (ftdi);
   
   return;
}

/**
   Auxiliary function used to abort program execution if a communication error is received. 
   Prints information about the error.
   
   @param ftdi pointer to struct ftdi_context
   @param error_string string containing error information
   @param error_code error code value
*/
void ftdi_exit (struct ftdi_context *ftdi, char *error_string, int error_code)
{
   fprintf (stderr, error_string, error_code, ftdi_get_error_string (ftdi));
   
   ftdi_free (ftdi);
   exit (EXIT_FAILURE);
}

/** 
   Writes data (command) to FTDI device, then checks if the device has received a bad command or not.
   
   @param ftdi pointer to struct ftdi_context
   @param data byte array (to read written data from)
   @param size size of data array
   
   @retval 0 if a bad command has been received
   @retval <0 if communication error
   @retval >0 if success (number of bytes written). 
*/
int ftdi_write_data_and_check (struct ftdi_context *ftdi, byte *data, int size)
{
   int ret, written_offset;
   byte buf[2];
   
   if ((ret = ftdi_write_data (ftdi, data, size)) < 0)
      return ret;
   
   /* save written offset */
   written_offset = ret;
   
   /* read two bytes */
   if ((ret = ftdi_read_data (ftdi, buf, 2)) < 0)
      return ret;
   
   /* check if ftdi received an invalid command */
   if (buf[0] == FTDI_BAD_COMMAND)
   {
      printf ("WARNING: Device received an invalid command: 0x%X\n", buf[1]);
      return 0;
   }
   
   return written_offset;    /* returns written data offset */
}

/**
   Reads data from FTDI device or waits if data is not available yet.

   @param ftdi pointer to struct ftdi_context
   @param data byte array (to write read data to)
   @param size size of data array
   
   @retval <0 if communication error
   @retval >0 if success (number of bytes read). 
*/
int ftdi_read_data_and_wait (struct ftdi_context *ftdi, byte *data, int size)
{
   int ret;
   
   int curr_size = size;

   while (curr_size > 0)
   {
      if ((ret = ftdi_read_data (ftdi, data, curr_size)) < 0)
         return ret;

      data += ret;
      curr_size -= ret;
   }

   return size;
}

/**
   Writes data to FTDI device or waits if data cannot be written yet.

   @param ftdi pointer to struct ftdi_context
   @param data byte array (to read written data from)
   @param size size of data array
   
   @retval <0 if communication error
   @retval >0 if success (number of bytes written). 
*/
int ftdi_write_data_and_wait (struct ftdi_context *ftdi, byte *data, int size)
{
   int ret;
   
   int curr_size = size;

   while (curr_size > 0)
   {
      if ((ret = ftdi_write_data (ftdi, data, curr_size)) < 0)
         return ret;

      data += ret;
      curr_size -= ret;
   }

   return size;
}

/**
   Reads FTDI modem status to check if transmitter buffer is empty.
   
   @param ftdi pointer to struct ftdi_context
   @param status pointer to a word variable (can be NULL if storing a new modem status is not necessary)

   @retval 1 if empty (TEMT = 1)
   @retval 0 if not empty (TEMT = 0)
   @retval <0 if cannot poll modem status
*/
int ftdi_tx_buf_empty (struct ftdi_context *ftdi, word *status)
{
   int ret;
   word temp_status;
   
   if ((ret = ftdi_poll_modem_status (ftdi, &temp_status) < 0))
      ftdi_exit (ftdi, "ERROR: Unable to poll modem status: %d (%s)\n", ret);
   
   if (status != NULL)
      *status = temp_status;

   return (temp_status & TEMT);
}

/**
   Reads FTDI modem status to check if any transmission error has occurred.

   @param ftdi pointer to struct ftdi_context
   @param status pointer to a word variable (can be NULL if storing a new modem status is not necessary)   
   
   @retval 1 if no error occurred (OE = PE = FE = 0)
   @retval 0 if an error occurred (OE or PE or FE = 1)
   @retval <0 if cannot poll modem status
*/
int ftdi_tx_error (struct ftdi_context *ftdi, word *status)
{
   int ret;
   word temp_status;

   if ((ret = ftdi_poll_modem_status (ftdi, &temp_status) < 0))
      ftdi_exit (ftdi, "ERROR: Unable to poll modem status: %d (%s)\n", ret);
   
   if (status != NULL)
      *status = temp_status;

   return !(temp_status & OE || temp_status & PE || temp_status & FE);
}