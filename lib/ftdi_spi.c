#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libftdi1\ftdi.h>

#include "ftdi_interface.h"
#include "ftdi_spi.h"

struct spi_context spi_init (struct ftdi_context *ftdi,
                             int clock_idle, int clock_phase, unsigned short int clock_divisor, int clock_divide_by_5, int write_lsb_first, int read_lsb_first, int mosi_idle, int miso_idle)
{
   /* Initialises spi communication on ftdi device, using user-defined parameters.
      Returns initialised spi context structure. */
   int ret;
   unsigned char buf[3];
   struct spi_context spi;

   /* init SPI structure */
   spi.CPOL = clock_idle & 1;
   spi.CPHA = clock_phase & 1;
   spi.CDIV = clock_divisor;
   spi.CDIV5 = clock_divide_by_5 & 1;
   spi.WRITE_LSB_FIRST = write_lsb_first & 1;
   spi.READ_LSB_FIRST = read_lsb_first & 1;
   spi.MOSI_IDLE = mosi_idle & 1;
   spi.MISO_IDLE = miso_idle & 1;

   /* purge all buffers */
   if ((ret = ftdi_usb_purge_buffers (ftdi)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to purge buffers: %d (%s)\n", ret);

   /* reset bitmode */
   if ((ret = ftdi_set_bitmode (ftdi, 0x00, BITMODE_RESET)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to reset bitmode: %d (%s)\n", ret);
   
   /* set MPSSE bitmode: GPIO not used, set to output */
   if ((ret = ftdi_set_bitmode (ftdi, 0x00, BITMODE_MPSSE)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to set MPSSE bitmode: %d (%s)\n", ret);

   /* synchronize MPSSE interface by sending a bad command */
   buf[0] = 0xAA;
   if ((ret = ftdi_write_data_and_check (ftdi, buf, 1)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to synchronize mpsse interface: %d (%s)\n", ret);
   else if (ret == 0)
      printf ("FTDI: MPSSE interface synchronized using 0xAA command\n");
   
   /* tune clock signal */
   buf[0] = spi.CDIV5 ? EN_DIV_5 : DIS_DIV_5;      /* enable/disable clock division by 5 */
   buf[1] = DIS_ADAPTIVE;                          /* ensure adaptive clocking is disabled */
   buf[2] = DIS_3_PHASE;                           /* ensure three-phase data clock is disabled */
   if ((ret = ftdi_write_data (ftdi, buf, 3)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to tune clock signal: %d (%s)\n", ret);
   
   printf ("FTDI: Clock divide by 5: ");
   spi.CDIV5 ? printf ("on\n") : printf ("off\n");
   
   /* set port direction and idle values */
   buf[0] = SET_BITS_LOW;                          /* set port direction and (initial) values */
   buf[1] = (spi.CPOL      ? SCLK : 0) |           /* set port idle values */
            (spi.MOSI_IDLE ? MOSI : 0) | 
            (spi.MISO_IDLE ? MISO : 0) | 
            CS;
   buf[2] = 0x0B;                                  /* GPIOL[3-0]: DC, CS: O, DI: I, DO: O, SK: O */
   if ((ret = ftdi_write_data (ftdi, buf, 3)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to set bits: %d (%s)\n", ret);
   
   printf ("FTDI: Clock idle level: ");
   spi.CPOL ? printf ("1\n") : printf ("0\n");
   
   buf[0] = TCK_DIVISOR;                           /* set clock divisor */
   buf[1] = GETBYTE (spi.CDIV, 0); 
   buf[2] = GETBYTE (spi.CDIV, 1);
   if ((ret = ftdi_write_data (ftdi, buf, 3)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to set clock divisor: %d (%s)\n", ret);

   spi_print_clk_frequency (spi);
   
   /* AN_114 note */
   usleep (20000);
   
   buf[0] = LOOPBACK_END;                          /* disable internal loopback */
   if ((ret = ftdi_write_data (ftdi, buf, 1)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to disable loopback: %d (%s)\n", ret);
   
   /* AN_114 note */
   usleep (30000);
   
   printf ("FTDI: SPI mode %d initialised!\n", (spi.CPOL << 1 | spi.CPHA));
   
   return spi;
}

void spi_open (struct ftdi_context *ftdi, struct spi_context spi)
{
   /* Opens spi connection by setting CS# line low. */
   unsigned char buf[3];
   int ret;
   
   buf[0] = SET_BITS_LOW;                    /* set port direction and values */
   buf[1] = (spi.MOSI_IDLE ? MOSI : 0) |     /* set MISO/MOSI idle values, CS=0 */
            (spi.MISO_IDLE ? MISO : 0);        
   if (!spi.CPHA)                            /* set clock idle polarity */
      buf[1] |= spi.CPOL  ? SCLK : 0;
   else
      buf[1] |= !spi.CPOL ? SCLK : 0;        /* workaround to get SPI mode 1, 3 working (invert clock polarity before writing data) */
   buf[2] = 0x0B;                            /* GPIOL[3-0]: DC, CS: O, DI: I, DO: O, SK: O */
   
   if ((ret = ftdi_write_data (ftdi, buf, 3)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to open SPI connection: %d (%s)\n", ret);

   return;
}

void spi_close (struct ftdi_context *ftdi, struct spi_context spi)
{
   /* Closes spi connection by setting CS# line high. */
   unsigned char buf[3];
   int ret;

   buf[0] = SET_BITS_LOW;                    /* Set port direction and (initial) value */
   buf[1] = (spi.CPOL      ? SCLK : 0) | 
            (spi.MOSI_IDLE ? MOSI : 0) | 
            (spi.MISO_IDLE ? MISO : 0) | 
            CS;                              /* set port idle values */
   buf[2] = 0x0B;                            /* GPIOL[3-0]: DC, CS: O, DI: I, DO: O, SK: O */

   if ((ret = ftdi_write_data (ftdi, buf, 3)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to close SPI connection: %d (%s)\n", ret);
   
   return;
}

void spi_write_from_file (struct ftdi_context *ftdi, struct spi_context spi, FILE *fp, int data_length)
{
   /* Sends data read from a file via spi on ftdi device. */
   unsigned char buffer[MAX_INTERNAL_BUF_LENGTH];       /* transmit buffer */
   
   int data_block_length;
   int i = 0;
   
   int part_of_frame = HEADER_0;
   int offset = 0;

   int ret;
   
   /* while there are still data to write out */
   while (data_length)
   {
      /* build up transmit buffer */
      
      /* must not exceed internal buf size; end if no more data in block is available */
      while (i < MAX_INTERNAL_BUF_LENGTH && data_length)
      {
         switch (part_of_frame)
         {
            case HEADER_0:
               buffer[i] = MPSSE_DO_WRITE | (MPSSE_LSB & spi.READ_LSB_FIRST);
               /* set spi mode according to AN_108 */
               if (SPIMODE (spi) == 0 || SPIMODE (spi) == 3)      /* mode 0 or mode 3 */
                  buffer[i] |= MPSSE_WRITE_NEG;

               part_of_frame = HEADER_1;
               break;
            case HEADER_1:
               /* get data length for one block */
               if (data_length > MAX_SPI_BUF_LENGTH)
                  data_block_length = MAX_SPI_BUF_LENGTH;
               else
                  data_block_length = data_length;
            
               buffer[i] = GETBYTE (data_block_length - 1, 0);         /* length (low byte) */

               part_of_frame = HEADER_2;
               break;
            case HEADER_2:
               buffer[i] = GETBYTE (data_block_length - 1, 1);         /* length (high byte) */

               part_of_frame = DATA;
               break;
            case DATA:
               /* decrease number of data in block and total number of data */
               data_length--;
               data_block_length--;
               
               /* load next data */
               if (fread (buffer + i, sizeof (char), 1, fp) < 1)
               {
                  printf ("WARNING: Cannot read file, end-of-file reached at byte %d\n", data_length);
                  data_length = 0;
               }
               
               if (data_block_length == 0)
                  part_of_frame = HEADER_0;
               break;
         }

         i++;
      }

      /* buffer is full or there is no more data to load */

      /* write out spi data */
      if ((ret = ftdi_write_data_and_wait (ftdi, buffer + offset, i - offset)) < 0)
         ftdi_exit (ftdi, "ERROR: Unable to send SPI data: %d (%s)\n", ret);

      /* if buffer's last elements are a part of header, these cannot be pushed out 
      if device hasn't received any spi data */
      if (part_of_frame != DATA)
         offset = part_of_frame;
      else
         offset = 0;

      /* buffer may be full: reset its index */
      /* if it's not full, there is no more data to send, so it can be reset in any case */
      i = offset;
   }

   return;
}

void spi_read_to_file (struct ftdi_context *ftdi, struct spi_context spi, FILE *fp, int data_length)
{
   /* Reads data via spi from ftdi device and saves them in a file. */
   const int HEADER_BLOCK_LENGTH = 3;                    /* length of header for one block of data */
   
   unsigned char file_buffer[MAX_SPI_BUF_LENGTH];        /* buffer used to store data before writing them out in a file */
   unsigned char buffer[MAX_INTERNAL_BUF_LENGTH];        /* transmit buffer */

   int frame_length, data_in_buffer_length, data_block_length;
   int i = 0;

   int part_of_frame = HEADER_0; 
   int offset = 0;
   
   int ret;
   
   /* get total frame length */
   frame_length = (data_length / MAX_SPI_BUF_LENGTH) * HEADER_BLOCK_LENGTH;
   if (data_length % MAX_SPI_BUF_LENGTH != 0)
      frame_length += HEADER_BLOCK_LENGTH;

   /* while there are still frames to send */
   while (frame_length)
   {
      data_in_buffer_length = 0;
      /* must not exceed internal buf size; end if no more frames are available */
      while (i < MAX_INTERNAL_BUF_LENGTH && frame_length)
      {
         switch (part_of_frame)
         {
            case HEADER_0:
               buffer[i] = MPSSE_DO_READ | (MPSSE_LSB & spi.READ_LSB_FIRST);
               /* set spi mode according to AN_108 */
               if (SPIMODE (spi) == 1 || SPIMODE (spi) == 2)      /* mode 1 or mode 2 */
                  buffer[i] |= MPSSE_READ_NEG;

               part_of_frame = HEADER_1;
               break;
            case HEADER_1:
               /* get data length for one block */
               if (data_length > MAX_SPI_BUF_LENGTH)
                  data_block_length = MAX_SPI_BUF_LENGTH;
               else
                  data_block_length = data_length;

               buffer[i] = GETBYTE (data_block_length - 1, 0);         /* length (low byte) */

               part_of_frame = HEADER_2;
               break;
            case HEADER_2:
               buffer[i] = GETBYTE (data_block_length - 1, 1);         /* length (high byte) */

               /* decrease total number of data to read */
               data_length -= data_block_length;
               /* increase number of data to read after sending this buffer */
               data_in_buffer_length += data_block_length;

               part_of_frame = HEADER_0;
               break;
         }

         i++;
         frame_length--;
      }

      /* write out spi data */
      if ((ret = ftdi_write_data_and_wait (ftdi, buffer + offset, i - offset)) < 0)
         ftdi_exit (ftdi, "ERROR: Unable to send SPI data: %d (%s)\n", ret);

      /* if buffer's last elements are a part of header, these cannot be pushed out 
      if device hasn't received any spi data */
      offset = part_of_frame;

      /* buffer may be full: reset its index */
      /* if it's not full, there is no more frames to send, so it can be reset in any case */
      i = offset;

      /* read data in blocks */
      while (data_in_buffer_length > 0)
      {
         if (data_in_buffer_length > MAX_SPI_BUF_LENGTH)
            data_block_length = MAX_SPI_BUF_LENGTH;
         else
            data_block_length = data_in_buffer_length;

         if ((ret = ftdi_read_data_and_wait (ftdi, file_buffer, data_block_length)) < 0)
            ftdi_exit (ftdi, "ERROR: Unable to read SPI data: %d (%s)\n", ret);

         fwrite (file_buffer, sizeof (char), data_block_length, fp);
         data_in_buffer_length -= data_block_length;
      }
   }

   return;
}

void spi_write (struct ftdi_context *ftdi, struct spi_context spi, unsigned char *data, int data_length)
{
   /* Sends data via spi on ftdi device. */
   unsigned char buffer[MAX_INTERNAL_BUF_LENGTH];       /* transmit buffer */
   
   int data_block_length;
   int i = 0;
   
   int part_of_frame = HEADER_0;
   int offset = 0;

   int ret;

   /* while there are still data to write out */
   while (data_length)
   {
      /* build up transmit buffer */
      
      /* must not exceed internal buf size; end if no more data in block is available */
      while (i < MAX_INTERNAL_BUF_LENGTH && data_length)
      {
         switch (part_of_frame)
         {
            case HEADER_0:
               buffer[i] = MPSSE_DO_WRITE | (MPSSE_LSB & spi.READ_LSB_FIRST);
               /* set spi mode according to AN_108 */
               if (SPIMODE (spi) == 0 || SPIMODE (spi) == 3)      /* mode 0 or mode 3 */
                  buffer[i] |= MPSSE_WRITE_NEG;

               part_of_frame = HEADER_1;
               break;
            case HEADER_1:
               /* get data length for one block */
               if (data_length > MAX_SPI_BUF_LENGTH)
                  data_block_length = MAX_SPI_BUF_LENGTH;
               else
                  data_block_length = data_length;
            
               buffer[i] = GETBYTE (data_block_length - 1, 0);         /* length (low byte) */

               part_of_frame = HEADER_2;
               break;
            case HEADER_2:
               buffer[i] = GETBYTE (data_block_length - 1, 1);         /* length (high byte) */

               part_of_frame = DATA;
               break;
            case DATA:
               buffer[i] = *data;

               /* decrease number of data in block and total number of data */
               data_length--;
               data_block_length--;
               
               /* load next data */
               data++;
               
               if (data_block_length == 0)
                  part_of_frame = HEADER_0;
               break;
         }

         i++;
      }

      /* buffer is full or there is no more data to load */
  
      /* write out spi data */
      if ((ret = ftdi_write_data_and_wait (ftdi, buffer + offset, i - offset)) < 0)
         ftdi_exit (ftdi, "ERROR: Unable to send SPI data: %d (%s)\n", ret);

      /* if buffer's last elements are a part of header, these cannot be pushed out 
      if device hasn't received any spi data */
      if (part_of_frame != DATA)
         offset = part_of_frame;
      else
         offset = 0;

      /* buffer may be full: reset its index */
      /* if it's not full, there is no more data to send, so it can be reset in any case */
      i = offset;
   }

   return;
}

void spi_read (struct ftdi_context *ftdi, struct spi_context spi, unsigned char *data, int data_length)
{
   /* Reads data via spi from ftdi device. */
   const int HEADER_BLOCK_LENGTH = 3;                    /* length of header for one block of data */

   unsigned char buffer[MAX_INTERNAL_BUF_LENGTH];        /* transmit buffer */

   int frame_length, data_in_buffer_length, data_block_length;
   int i = 0;
   
   int part_of_frame = HEADER_0; 
   int offset = 0;

   int ret;

   /* get total frame length */
   frame_length = (data_length / MAX_SPI_BUF_LENGTH) * HEADER_BLOCK_LENGTH;
   if (data_length % MAX_SPI_BUF_LENGTH != 0)
      frame_length += HEADER_BLOCK_LENGTH;

   /* while there are still frames to send */
   while (frame_length)
   {
      data_in_buffer_length = 0;
      /* must not exceed internal buf size; end if no more frames are available */
      while (i < MAX_INTERNAL_BUF_LENGTH && frame_length)
      {
         switch (part_of_frame)
         {
            case HEADER_0:
               buffer[i] = MPSSE_DO_READ | (MPSSE_LSB & spi.READ_LSB_FIRST);
               /* set spi mode according to AN_108 */
               if (SPIMODE (spi) == 1 || SPIMODE (spi) == 2)      /* mode 1 or mode 2 */
                  buffer[i] |= MPSSE_READ_NEG;

               part_of_frame = HEADER_1;
               break;
            case HEADER_1:
               /* get data length for one block */
               if (data_length > MAX_SPI_BUF_LENGTH)
                  data_block_length = MAX_SPI_BUF_LENGTH;
               else
                  data_block_length = data_length;

               buffer[i] = GETBYTE (data_block_length - 1, 0);         /* length (low byte) */

               part_of_frame = HEADER_2;
               break;
            case HEADER_2:
               buffer[i] = GETBYTE (data_block_length - 1, 1);         /* length (high byte) */

               /* decrease total number of data to read */
               data_length -= data_block_length;
               /* increase number of data to read after sending this buffer */
               data_in_buffer_length += data_block_length;

               part_of_frame = HEADER_0;
               break;
         }

         i++;
         frame_length--;
      }

      /* write out spi data */
      if ((ret = ftdi_write_data_and_wait (ftdi, buffer + offset, i - offset)) < 0)
         ftdi_exit (ftdi, "ERROR: Unable to send SPI data: %d (%s)\n", ret);

      /* if buffer's last elements are a part of header, these cannot be pushed out 
      if device hasn't received any spi data */
      offset = part_of_frame;

      /* buffer may be full: reset its index */
      /* if it's not full, there is no more frames to send, so it can be reset in any case */
      i = offset;

      /* read data */
      if ((ret = ftdi_read_data_and_wait (ftdi, data, data_in_buffer_length)) < 0)
            ftdi_exit (ftdi, "ERROR: Unable to read SPI data: %d (%s)\n", ret);

      data += data_in_buffer_length;
   }

   return;
}


void spi_print_clk_frequency (struct spi_context spi)
{
   /* Prints selected spi clock frequency to the terminal screen */
   double clock_frequency;
   char prefix = '\0';
   
   if (spi.CDIV5)
      clock_frequency = 12e6 / ((1 + (double)spi.CDIV) * 2);
   else
      clock_frequency = 60e6 / ((1 + (double)spi.CDIV) * 2);
   
   /* normalise using engineering notation */
   if      (clock_frequency >= 1e6) { clock_frequency /= 1e6; prefix = 'M'; }
   else if (clock_frequency >= 1e3) { clock_frequency /= 1e3; prefix = 'k'; }
   
   printf ("FTDI: Clock frequency set to %.3f ", clock_frequency);
   
   prefix ? printf ("%cHz\n", prefix) : printf ("Hz\n");
   
   return;
}