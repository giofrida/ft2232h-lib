#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <libftdi1\ftdi.h>

#include "ftdi_interface.h"
#include "ftdi_spi.h"

/** 
   Initialises SPI communication on FTDI device, using user-defined parameters.
   
   @param ftdi pointer to struct ftdi_context
   
   @param clock_idle clock idle level (also known as clock polarity, CPOL)
   @param clock_phase clock phase (CPHA)
   @param clock_divisor clock divisor value
   @param clock_divide_by_5 enable clock divide by 5
   @param mosi_idle MOSI idle level
   @param write_lsb_first write lsb first
   @param read_lsb_first read lsb first
   @param loopback_on internal loopback on
   
   @return pointer to initialised spi_context structure  
*/
struct spi_context *spi_init (struct ftdi_context *ftdi,
                             int clock_idle, int clock_phase, word clock_divisor, int clock_divide_by_5, int mosi_idle, int write_lsb_first, int read_lsb_first, int loopback_on)
{
   int ret;
   byte buf[3];
   byte level, io;
   struct spi_context *spi;

   /* allocate structure */
   spi = (struct spi_context *)malloc (sizeof (struct spi_context));
   
   /* init SPI structure */
   spi->CPOL = clock_idle & 1;
   spi->CPHA = clock_phase & 1;
   spi->CDIV = clock_divisor;
   spi->CDIV5 = clock_divide_by_5 & 1;
   spi->MOSI_IDLE = mosi_idle & 1;
   spi->WRITE_LSB_FIRST = write_lsb_first & 1;
   spi->READ_LSB_FIRST = read_lsb_first & 1;
   spi->LOOPBACK_ON = loopback_on & 1;

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
   buf[0] = spi->CDIV5 ? EN_DIV_5 : DIS_DIV_5;      /* enable/disable clock division by 5 */
   buf[1] = DIS_ADAPTIVE;                          /* ensure adaptive clocking is disabled */
   buf[2] = DIS_3_PHASE;                           /* ensure three-phase data clock is disabled */
   if ((ret = ftdi_write_data (ftdi, buf, 3)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to tune clock signal: %d (%s)\n", ret);
   
   printf ("FTDI: Clock divide by 5: ");
   spi->CDIV5 ? printf ("enabled\n") : printf ("disabled\n");

   buf[0] = TCK_DIVISOR;                           /* set clock divisor */
   buf[1] = GETBYTE (spi->CDIV, 0); 
   buf[2] = GETBYTE (spi->CDIV, 1);
   if ((ret = ftdi_write_data (ftdi, buf, 3)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to set clock divisor: %d (%s)\n", ret);

   spi_print_clk_frequency (spi);

   /* AN_114 note */
   usleep (20000);
   
   if (spi->LOOPBACK_ON)
   {
      /* enable internal loopback */
      buf[0] = LOOPBACK_START;
      if ((ret = ftdi_write_data (ftdi, buf, 1)) < 0)
         ftdi_exit (ftdi, "ERROR: Unable to enable loopback: %d (%s)\n", ret);
   }
   else
   {
      /* disable internal loopback */
      buf[0] = LOOPBACK_END;
      if ((ret = ftdi_write_data (ftdi, buf, 1)) < 0)
         ftdi_exit (ftdi, "ERROR: Unable to disable loopback: %d (%s)\n", ret);
   }
   printf ("FTDI: SPI loopback: ");
   spi->LOOPBACK_ON ? printf ("enabled\n") : printf ("disabled\n");
   
   /* set port direction and idle values */
   level = (spi->CPOL      ? SCLK : 0) |           /* set port idle values */
           (spi->MOSI_IDLE ? MOSI : 0) | 
           CS;
   io = 0x0B;                                     /* GPIOL[3-0]: DC, CS: O, DI: I, DO: O, SK: O */
   
   ftdi_set_bits_low (ftdi, spi, 0xFF, level, io);
   ftdi_set_bits_high (ftdi, spi, 0xFF, 0xFF, 0xFF);    /* high bits set to high, as outputs, by default */
   
   printf ("FTDI: Clock idle level: %d, MOSI idle level: %d\n", spi->CPOL, spi->MOSI_IDLE);
         
   /* AN_114 note */
   usleep (30000);
   
   printf ("FTDI: SPI mode %d initialised!\n", SPIMODE (spi));
   
   return spi;
}

/** 
   Opens SPI connection by setting CS# line low. Also, sets MOSI and SCLK lines to their respective idle levels. 
   
   @param ftdi pointer to struct ftdi_context
   @param spi pointer to struct spi_context
*/
void spi_open (struct ftdi_context *ftdi, struct spi_context *spi)
{
   byte level;
   
   level = (spi->MOSI_IDLE ? MOSI : 0);       /* set MOSI idle value, CS=0 */
   if (!spi->CPHA)                            /* set clock idle polarity */
      level |= (spi->CPOL  ? SCLK : 0);       /* AN_108: clock out on -ve (mode 0) requires SCLK=0, clock out on +ve (mode 2) requires SCLK=1 */
   else                                       /* workaround to get SPI mode 1, 3 working (invert clock polarity before writing data) */
      level |= (!spi->CPOL ? SCLK : 0);       /* AN_108: clock out on +ve (mode 2) requires SCLK=1, clock out on -ve (mode 3) requires SCLK=0 */      
      
   ftdi_set_bits_low (ftdi, spi, CS|SCLK|MOSI, level, CS|SCLK|MOSI);

   DEBUG_PRINT ("DEBUG: [SPI] Asserting CS#\n");
   
   return;
}

/** 
   Closes SPI connection by setting CS# line high. Also, sets MOSI and SCLK lines to their respective idle levels. 
   
   @param ftdi pointer to struct ftdi_context
   @param spi pointer to struct spi_context
*/
void spi_close (struct ftdi_context *ftdi, struct spi_context *spi)
{
   byte level;

   level = (spi->CPOL      ? SCLK : 0) | 
           (spi->MOSI_IDLE ? MOSI : 0) |
           CS;                              /* set port idle values */
           
   ftdi_set_bits_low (ftdi, spi, CS|SCLK|MOSI, level, CS|SCLK|MOSI);
   
   DEBUG_PRINT ("DEBUG: [SPI] De-asserting CS#\n\n");
   
   return;
}

/**
   Sends data read from the file pointed by fp via SPI on FTDI device.
   
   @param ftdi pointer to struct ftdi_context
   @param spi pointer to struct spi_context
   @param fp pointer to FILE
   @param size size of data to write
   
   @retval <0 if EOF reached before sending out all data
   @retval >0 on success
*/
int spi_write_from_file (struct ftdi_context *ftdi, struct spi_context *spi, FILE *fp, int size)
{
   byte buf[3];
   byte file_buf[MAX_SPI_BUF_LENGTH];
   unsigned int buf_size;
   int ret;
   
   if (size > MAX_SPI_BUF_LENGTH)
      buf_size = MAX_SPI_BUF_LENGTH;
   else
      buf_size = size; 
   
   while (size > 0 && fread (file_buf, sizeof (byte), buf_size, fp) == buf_size)
   {     
      /* build header */
      buf[0] = MPSSE_DO_WRITE | (spi->WRITE_LSB_FIRST ? MPSSE_LSB : 0);
      /* set spi mode according to AN_108 */
      if (SPIMODE (spi) == 0 || SPIMODE (spi) == 3)      /* mode 0 or mode 3 (clock out on -ve) */
         buf[0] |= MPSSE_WRITE_NEG;
      buf[1] = GETBYTE (buf_size - 1, 0);         /* length (low byte) */
      buf[2] = GETBYTE (buf_size - 1, 1);         /* length (high byte) */
      
      /* write out header */
      if ((ret = ftdi_write_data_and_wait (ftdi, buf, 3)) < 0)
         ftdi_exit (ftdi, "ERROR: Unable to send SPI data: %d (%s)\n", ret);
      /* write out spi data */
      if ((ret = ftdi_write_data_and_wait (ftdi, file_buf, buf_size)) < 0)
         ftdi_exit (ftdi, "ERROR: Unable to send SPI data: %d (%s)\n", ret);
      
      size -= buf_size;
      
      if (size > MAX_SPI_BUF_LENGTH)
         buf_size = MAX_SPI_BUF_LENGTH;
      else
         buf_size = size;
   }
   
   if (feof (fp))
   {
      printf ("WARNING: Cannot read file, end-of-file reached before sending out all data!\n");
      printf ("   Remaining size: %d bytes\n", size);
      return -1;
   }
   
   return 1;
}


/** 
   Reads data via SPI from FTDI device and saves them in the file pointed by fp. 
   
   @param ftdi pointer to struct ftdi_context
   @param spi pointer to struct spi_context
   @param fp pointer to FILE
   @param size size of data to read
   
   @retval <0 if a write error occurred before reading in all data
   @retval >0 on success
*/
int spi_read_to_file (struct ftdi_context *ftdi, struct spi_context *spi, FILE *fp, int size)
{
   byte buf[3];
   byte file_buf[MAX_SPI_BUF_LENGTH];
   unsigned int buf_size;
   int ret;
   
   while (size > 0)
   {
      if (size > MAX_SPI_BUF_LENGTH)
         buf_size = MAX_SPI_BUF_LENGTH;
      else
         buf_size = size;
      
      buf[0] = MPSSE_DO_READ | (spi->READ_LSB_FIRST ? MPSSE_LSB : 0);
      /* set spi mode according to AN_108 */
      if (SPIMODE (spi) == 1 || SPIMODE (spi) == 2)      /* mode 1 or mode 2 (clock out on +ve) */
         buf[0] |= MPSSE_READ_NEG;
      buf[1] = GETBYTE (buf_size - 1, 0);         /* length (low byte) */
      buf[2] = GETBYTE (buf_size - 1, 1);         /* length (high byte) */
      
      /* write out spi data */
      if ((ret = ftdi_write_data_and_wait (ftdi, buf, 3)) < 0)
         ftdi_exit (ftdi, "ERROR: Unable to send SPI data: %d (%s)\n", ret);

      /* read data */
      if ((ret = ftdi_read_data_and_wait (ftdi, file_buf, buf_size)) < 0)
            ftdi_exit (ftdi, "ERROR: Unable to read SPI data: %d (%s)\n", ret);
      
      /* write read data to file */
      if (fwrite (file_buf, sizeof (byte), buf_size, fp) != buf_size)
      {
         fprintf (stderr, "ERROR: Unable to write file!\n");
         printf ("   Remaining size: %d bytes\n", size);
         return -1;
      }
      
      size -= buf_size;
   }
 
   return 1;
}

/**
   Sends data via SPI on FTDI device.
   
   @param ftdi pointer to struct ftdi_context
   @param spi pointer to struct spi_context
   @param data byte array with data to write
   @param size size of data to write
*/

void spi_write (struct ftdi_context *ftdi, struct spi_context *spi, byte *data, int size)
{
   byte buf[3];
   byte *curr_data;
   int buf_size, rem_size;
   int ret;
   
   rem_size = size;
   curr_data = data;
   
   while (rem_size > 0)
   {
      if (rem_size > MAX_SPI_BUF_LENGTH)
         buf_size = MAX_SPI_BUF_LENGTH;
      else
         buf_size = rem_size;
      
      /* build header */
      buf[0] = MPSSE_DO_WRITE | (spi->WRITE_LSB_FIRST ? MPSSE_LSB : 0);
      /* set spi mode according to AN_108 */
      if (SPIMODE (spi) == 0 || SPIMODE (spi) == 3)      /* mode 0 or mode 3 (clock out on -ve) */
         buf[0] |= MPSSE_WRITE_NEG;
      buf[1] = GETBYTE (buf_size - 1, 0);         /* length (low byte) */
      buf[2] = GETBYTE (buf_size - 1, 1);         /* length (high byte) */
      
      /* write out header */
      if ((ret = ftdi_write_data_and_wait (ftdi, buf, 3)) < 0)
         ftdi_exit (ftdi, "ERROR: Unable to send SPI data: %d (%s)\n", ret);
      /* write out spi data */
      if ((ret = ftdi_write_data_and_wait (ftdi, curr_data, buf_size)) < 0)
         ftdi_exit (ftdi, "ERROR: Unable to send SPI data: %d (%s)\n", ret);
           
      rem_size -= buf_size;
      curr_data += buf_size;
   }
   
#ifdef DEBUG
   int i;
   DEBUG_PRINT ("DEBUG: [SPI] Sending ");
   for (i = 0; i < size; i++)
      DEBUG_PRINT ("%.2X ", data[i]);
   DEBUG_PRINT ("\n");
#endif
  
   return;
}

/**
   Reads data via SPI from FTDI device.
   
   @param ftdi pointer to struct ftdi_context
   @param spi pointer to struct spi_context
   @param data byte array to store data in
   @param size size of data to read
*/
void spi_read (struct ftdi_context *ftdi, struct spi_context *spi, byte *data, int size)
{
   byte buf[3];
   byte *curr_data;
   int buf_size, rem_size;
   int ret;
   
   rem_size = size;
   curr_data = data;
   
   while (rem_size > 0)
   {
      if (rem_size > MAX_SPI_BUF_LENGTH)
         buf_size = MAX_SPI_BUF_LENGTH;
      else
         buf_size = rem_size;
      
      buf[0] = MPSSE_DO_READ | (spi->READ_LSB_FIRST ? MPSSE_LSB : 0);
      /* set spi mode according to AN_108 */
      if (SPIMODE (spi) == 1 || SPIMODE (spi) == 2)      /* mode 1 or mode 2 (clock out on +ve) */
         buf[0] |= MPSSE_READ_NEG;
      buf[1] = GETBYTE (buf_size - 1, 0);         /* length (low byte) */
      buf[2] = GETBYTE (buf_size - 1, 1);         /* length (high byte) */
      
      /* write out spi data */
      if ((ret = ftdi_write_data_and_wait (ftdi, buf, 3)) < 0)
         ftdi_exit (ftdi, "ERROR: Unable to send SPI data: %d (%s)\n", ret);

      /* read data */
      if ((ret = ftdi_read_data_and_wait (ftdi, curr_data, buf_size)) < 0)
            ftdi_exit (ftdi, "ERROR: Unable to read SPI data: %d (%s)\n", ret);
         
      rem_size -= buf_size;
      curr_data += buf_size;
   }
   
#ifdef DEBUG
   int i;
   DEBUG_PRINT ("DEBUG: [SPI] Receiving ");
   for (i = 0; i < size; i++)
      DEBUG_PRINT ("%.2X ", data[i]);
   DEBUG_PRINT ("\n");
#endif
 
   return;
}

/**
   Prints selected SPI clock frequency to the terminal screen.
   
   @param spi pointer to struct spi_context
*/
void spi_print_clk_frequency (struct spi_context *spi)
{
   double clock_frequency;
   char prefix = '\0';

   clock_frequency = spi_frequency (spi);
   
   /* normalise using engineering notation */
   if      (clock_frequency >= 1e6) { clock_frequency /= 1e6; prefix = 'M'; }
   else if (clock_frequency >= 1e3) { clock_frequency /= 1e3; prefix = 'k'; }
   
   printf ("FTDI: Clock frequency set to %.3f ", clock_frequency);
   
   prefix ? printf ("%cHz\n", prefix) : printf ("Hz\n");
   
   return;
}

/**
   Calculates selected SPI clock frequency.
   
   @param spi pointer to struct spi_context
   
   @return clock frequency
*/
double spi_frequency (struct spi_context *spi)
{
   double clock_frequency;
   
   if (spi->CDIV5)
      clock_frequency = 12e6 / ((1 + (double)spi->CDIV) * 2);
   else
      clock_frequency = 60e6 / ((1 + (double)spi->CDIV) * 2);
   
   return clock_frequency;
}

/**
   Calculates selected SPI clock period.
   
   @param spi pointer to struct spi_context
   
   @return clock period
*/
double spi_clk_period (struct spi_context *spi)
{
   double clock_period;
   
   clock_period = 1.0 / spi_frequency (spi);
   
   return clock_period;
}

/**
   Sets level and i/o state of low bits in FTDI device. Only selected pins in mask will be modified.
   <br>Note that in SPI mode the four least significant bits controls SCLK, MOSI, MISO and CS lines 
   and should not be changed via this function unless particular initialisation are required.
   
   @param ftdi pointer to struct ftdi_context
   @param spi pointer to struct spi_context
   @param mask bit mask (set to 1 the pins whose level or i/o must be modified)
   @param level pins level (0 = low, 1 = high)
   @param io pins i/o (0 = output, 1 = input)
*/
void ftdi_set_bits_low (struct ftdi_context *ftdi, struct spi_context *spi, 
                                      byte mask, byte level, byte io)
{
   byte buf[3];
   int ret;

   spi->low_bits.level |= (mask & level);     /* sets to 1 selected bits */
   spi->low_bits.level &= (~mask | level);    /* sets to 0 selected bits */
   spi->low_bits.io |= (mask & io);           /* sets to 1 selected bits */
   spi->low_bits.io &= (~mask | io);          /* sets to 0 selected bits */
   
   /* set bits state */
   buf[0] = SET_BITS_LOW;
   buf[1] = spi->low_bits.level;
   buf[2] = spi->low_bits.io;
   
   if ((ret = ftdi_write_data (ftdi, buf, 3)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to set low bits: %d (%s)\n", ret);
   
   return;
}

/**
   Sets level and i/o state of high bits in FTDI device. Only selected pins in mask will be modified.
   
   @param ftdi pointer to struct ftdi_context
   @param spi pointer to struct spi_context
   @param mask bit mask (set to 1 the pins whose level or i/o must be modified)
   @param level pins level (0 = low, 1 = high)
   @param io pins i/o (0 = output, 1 = input)
*/
void ftdi_set_bits_high (struct ftdi_context *ftdi, struct spi_context *spi, 
                                       byte mask, byte level, byte io)
{
   byte buf[3];
   int ret;

   spi->high_bits.level |= (mask & level);     /* sets to 1 selected bits */
   spi->high_bits.level &= (~mask | level);    /* sets to 0 selected bits */
   spi->high_bits.io |= (mask & io);           /* sets to 1 selected bits */
   spi->high_bits.io &= (~mask | io);          /* sets to 0 selected bits */
   
   /* set bits state */
   buf[0] = SET_BITS_HIGH;
   buf[1] = spi->high_bits.level;
   buf[2] = spi->high_bits.io;

   if ((ret = ftdi_write_data (ftdi, buf, 3)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to set high bits: %d (%s)\n", ret);
   
   return;
}

/**
   Gets level of low bits in FTDI device. Data retrieved is used to update spi_context structure.
   
   @param ftdi pointer to struct ftdi_context
   @param spi pointer to struct spi_context

   @return level of low bits
*/
byte ftdi_get_bits_low (struct ftdi_context *ftdi, struct spi_context *spi)
{
   byte buf, level;
   int ret;
   
   /* get bits state */
   buf = GET_BITS_LOW;
   
   if ((ret = ftdi_write_data (ftdi, &buf, 1)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to request low bits level: %d (%s)\n", ret);
   if ((ret = ftdi_read_data (ftdi, &level, 1)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to get low bits level: %d (%s)\n", ret);
   
   spi->low_bits.level = level;
     
   return level;
}

/**
   Gets level of high bits in FTDI device. Data retrieved is used to update spi_context structure.
   
   @param ftdi pointer to struct ftdi_context
   @param spi pointer to struct spi_context

   @return level of high bits
*/
byte ftdi_get_bits_high (struct ftdi_context *ftdi, struct spi_context *spi)
{
   byte buf, level;
   int ret;
   
   /* get bits state */
   buf = GET_BITS_HIGH;
   
   if ((ret = ftdi_write_data (ftdi, &buf, 1)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to request high bits level: %d (%s)\n", ret);
   if ((ret = ftdi_read_data (ftdi, &level, 1)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to get high bits level: %d (%s)\n", ret);
   
   spi->high_bits.level = level;
     
   return level;
}