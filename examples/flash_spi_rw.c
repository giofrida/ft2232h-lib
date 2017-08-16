#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libftdi1\ftdi.h>
#include <ctype.h>

#include "..\lib\ftdi_interface.h"
#include "..\lib\ftdi_spi.h"

#include "spi_flash.h"

#define GETBIT(field, bit)        (((field) & (1 << (bit))) >> (bit))

#define BYTE_TO_BINARY_PATTERN "%5c%5c%5c%5c%5c%5c%5c%5c"
#define BYTE_TO_BINARY(byte)  \
  GETBIT (byte, 7), \
  GETBIT (byte, 6), \
  GETBIT (byte, 5), \
  GETBIT (byte, 4), \
  GETBIT (byte, 3), \
  GETBIT (byte, 2), \
  GETBIT (byte, 1), \
  GETBIT (byte, 0) 

  
dword read_eeprom_size (char *str);

void flash_read (struct ftdi_context *ftdi, struct spi_context *spi, FILE *fp, dword size);
int flash_write (struct ftdi_context *ftdi, struct spi_context *spi, FILE *fp, dword size);
int flash_verify (struct ftdi_context *ftdi, struct spi_context *spi, FILE *fp, dword size);
void flash_erase (struct ftdi_context *ftdi, struct spi_context *spi);

void flash_print_info (byte *eeprom_id);
void flash_id_manufacturer (byte id, char *man);
void flash_read_id (struct ftdi_context *ftdi, struct spi_context *spi, byte *id);

byte flash_read_status (struct ftdi_context *ftdi, struct spi_context *spi);
void flash_wait_if_busy (struct ftdi_context *ftdi, struct spi_context *spi);
void flash_write_enable (struct ftdi_context *ftdi, struct spi_context *spi);

time_t time_sync (void);


int main (int argc, char *argv[])
{
   struct ftdi_context *ftdi;
   struct spi_context *spi;
   
   FILE *fp_read, *fp_write;
   
   byte eeprom_id[3];
   dword EEPROM_SIZE;
   
   if (argc < 2)
   {
      fprintf (stderr, "ERROR: Missing arguments\n");
      fprintf (stderr, "    Usage: flash_spi_rw flash_size [write_file]\n");
      return EXIT_FAILURE;
   }
   
   if ((EEPROM_SIZE = read_eeprom_size (argv[1])) == 0)
   {
      fprintf (stderr, "ERROR: Invalid EEPROM size!\n");
      return EXIT_FAILURE;
   }
   
   /* init ftdi communication (usb paramters) */
   ftdi = ftdi_open ();
   
   /* init spi communication: spi mode 3, maximum divider, divide by 5 on, MSB first */
   spi = spi_init (ftdi, 1, 1, 0x0000, 0, 1, 0, 0, 0);
   
   /* From Macronix datasheet, device operation:
   1. Before issuing any command, check the status register to ensure device is ready
      for the intended operation.
   2. When an incorrect command is inputted to the chip, it goes in standby mode. The 
      chip is released from standby at the next CS# falling edge.
   3. When a correct command is inputted to the chip, it goes in active mode. The active
      mode is kept until the next CS# rising edge.
   4. Input data is latched on the rising edge of SCLK, and data shifts out on the falling
      edge of SCLK. Always MSB first, SPI modes supported: CPOL, SPHA = 00 or 11.
   5. For the read commands (RDID, RDSR, etc.) the instruction sequence is followed by a data
      out sequence. After any bit of data being shifted out, CS# can be high.
      For the write commands (WREN, WRDI, etc.), CS# must go high exactly at the byte boundary,
      otherwise the instruction will be rejected and not executed.
   6. During the operation of WRSR, PP, SE, BE, CE, the access to the memory array is ignored and
      does not affect the current operation. */
   
   /* read eeprom id and print information */
   flash_read_id (ftdi, spi, eeprom_id);
   flash_print_info (eeprom_id);
   
   /* read entire chip and save it in a file */
   if ((fp_read = fopen ("EEPROM_backup.bin", "wb")) == NULL) 
   {
      fprintf (stderr, "ERROR: File not found or not accessible!\n");
      spi_free (spi);
      ftdi_close (ftdi);
      return EXIT_FAILURE;
   }
   
   printf ("INFO: Reading EEPROM...\n");
   flash_read (ftdi, spi, fp_read, EEPROM_SIZE);
   printf ("INFO: EEPROM dumped in \'EEPROM_backup.bin\'\n");
   
   fclose (fp_read);
   
   if (argc < 3)
   {
      spi_free (spi);
      ftdi_close (ftdi);
      return EXIT_SUCCESS;
   }

   /* erase chip before writing */
   printf ("INFO: Erasing EEPROM...\n");
   flash_erase (ftdi, spi);
   printf ("INFO: EEPROM erased.\n");
   
   /* write entire chip from file */
   if ((fp_write = fopen (argv[2], "rb")) == NULL) 
   {
      fprintf (stderr, "ERROR: File not found or not accessible!\n");
      spi_free (spi);
      ftdi_close (ftdi);
      return EXIT_FAILURE;
   }
   
   printf ("INFO: Writing EEPROM...\n");
   if (flash_write (ftdi, spi, fp_write, EEPROM_SIZE) <= 0)
   {
      fprintf (stderr, "ERROR: Unable to write EEPROM!\n");
      fclose (fp_write);
      spi_free (spi);
      ftdi_close (ftdi);
      return EXIT_FAILURE;
   }
   printf ("INFO: Wrote EEPROM from file \'%s\'\n", argv[2]);
   
   /* verify eeprom */
   rewind (fp_write);
   printf ("INFO: Verifying EEPROM...\n");
   if (flash_verify (ftdi, spi, fp_write, EEPROM_SIZE) <= 0)
   {
      fprintf (stderr, "ERROR: Unable to verify EEPROM!\n");
      fclose (fp_write);
      spi_free (spi);
      ftdi_close (ftdi);
      return EXIT_FAILURE;
   }
   printf ("INFO: EEPROM verified.\n");
   
   fclose (fp_write);
   spi_free (spi);
   ftdi_close (ftdi);
   return EXIT_FAILURE;
}


dword read_eeprom_size (char *str)
{
   char mult;
   dword size;
   
   sscanf (str, "%d%c", &size, &mult);
   
   switch (tolower (mult))
   {
      case 'g': size *= 1024 * 1024 * 1024;
                break;
      case 'm': size *= 1024 * 1024;
                break;
      case 'k': size *= 1024;
                break;
   }
   
   return size;
}


void flash_read (struct ftdi_context *ftdi, struct spi_context *spi, FILE *fp, dword size)
{
   byte buf[4] = { READ, 0x00, 0x00, 0x00 };
   
   spi_open (ftdi, spi);
   spi_write (ftdi, spi, buf, 4);
   spi_read_to_file (ftdi, spi, fp, size);
   spi_close (ftdi, spi);
   
   return;
}

int flash_write (struct ftdi_context *ftdi, struct spi_context *spi, FILE *fp, dword size)
{
   byte buf[4] = { PP, 0x00, 0x00, 0x00 };
   byte file_buf[256];
   
   unsigned int file_buf_size;
   unsigned int addr, rem_size;

   time_t start, end;
   
   addr = 0x000000;
   rem_size = size;
   
   start = time_sync ();
   
   if (rem_size > 256)
      file_buf_size = 256;
   else
      file_buf_size = rem_size;
   
   while (addr < size && fread (file_buf, sizeof (byte), file_buf_size, fp) == file_buf_size)
   {
      /* load address */
      buf[1] = GETBYTE (addr, 2);
      buf[2] = GETBYTE (addr, 1);
      buf[3] = GETBYTE (addr, 0);
      
      /* ensure BUSY bit is cleared */
      flash_wait_if_busy (ftdi, spi);
      
      /* ensure WEL bit is set */
      flash_write_enable (ftdi, spi);
      
      spi_open (ftdi, spi);
      /* send write request */
      spi_write (ftdi, spi, buf, 4);
      /* send data to write out */
      spi_write (ftdi, spi, file_buf, file_buf_size);     
      spi_close (ftdi, spi);
      
      addr += file_buf_size;
      rem_size -= file_buf_size;
      
      if (rem_size > 256)
         file_buf_size = 256;
      else
         file_buf_size = rem_size;
      
      time (&end);
      if (difftime (end, start) == 1 || addr == size)
      {
         printf ("INFO: %.1f%% (%d bytes written)\n", 100.0 * addr / size, addr);
         time (&start);
      }
   }

   flash_wait_if_busy (ftdi, spi);
   
   /* if not all data has been written, because an EOF has occurred */
   if (addr < size && fread (file_buf, sizeof (byte), file_buf_size, fp) != 1)
   {
      printf ("WARNING: Cannot read file, end-of-file reached at address 0x%.6X\n", addr);
      return -1;
   }
   
   /* if all data has been written but there is still data in file, print warning */
   if (addr >= size && fread (file_buf, sizeof (byte), file_buf_size, fp) == 1)
   {
      printf ("WARNING: There is still data in file over 0x%.6X\n", addr);
   }
   
   return 1;
}

int flash_verify (struct ftdi_context *ftdi, struct spi_context *spi, FILE *fp, dword size)
{
   FILE *fp_temp;
   byte byte1, byte2;
   dword addr;
   
   /* open temporary file in write/update mode (both read and write) */
   if ((fp_temp = fopen ("temp.bin", "w+b")) == NULL)
   {
      fprintf (stderr, "ERROR: Cannot create file!");
      return -1;
   }
   /* read flash memory once again */
   flash_read (ftdi, spi, fp_temp, size);
   
   /* go back to the beginning of temporary */
   rewind (fp_temp);
   
   addr = 0x000000;
   /* compare the two files */
   while (addr < size && fread (&byte1, sizeof (byte), 1, fp) == 1)
   {  
      fread (&byte2, sizeof (byte), 1, fp_temp); 

      if (byte1 != byte2)
      {
         fprintf (stderr, "ERROR: Data mismatch at address 0x%.6X\n", addr);
         return 0;
      }

      addr++;
   }
   
   fclose (fp_temp);
   /* delete temporary file */
   if (remove ("temp.bin") != 0)
   {
      fprintf (stderr, "ERROR: Unable to delete temp file!\n");
      return -1;
   }
   
   /* if not all data has been verified, because an EOF has occurred */
   if (addr < size && fread (&byte1, sizeof (byte), 1, fp) != 1)
   {
      printf ("WARNING: Cannot read file, end-of-file reached at address 0x%.6X\n", addr);
      return -1;
   }
   
   /* if all data has been verified but there is still data in file, print warning */
   if (addr >= size && fread (&byte1, sizeof (byte), 1, fp) == 1)
   {
      printf ("WARNING: There is still data in file over 0x%.6X\n", addr);
   }

   return 1;
}

void flash_erase (struct ftdi_context *ftdi, struct spi_context *spi)
{
   byte buf = CE;
   
   flash_wait_if_busy (ftdi, spi);
   flash_write_enable (ftdi, spi);
   
   spi_open (ftdi, spi);
   spi_write (ftdi, spi, &buf, 1);
   spi_close (ftdi, spi);

   flash_wait_if_busy (ftdi, spi);
   
   return;
}


void flash_print_info (byte *eeprom_id)
{
   char manufacturer[64];
   
   flash_id_manufacturer (eeprom_id[0], manufacturer);
   
   printf ("INFO: EEPROM Identification data:\n");
   printf ("   Manufacturer ID: 0x%.2X (%s)\n", eeprom_id[0], manufacturer);
   printf ("       Memory type: 0x%.2X\n", eeprom_id[1]);
   printf ("    Memory density: 0x%.2X\n", eeprom_id[2]);
   
   return;
}

void flash_id_manufacturer (byte id, char *man)
{
   const struct flash_manufacturer flash_list[] = {
      {0x01,"AMD/Cypress/Spansion"},
      {0x04,"Fujitsu"},
      {0x1C,"EON"},
      {0x1F,"Atmel"},
      {0x20,"ST/SGS/Micron"},
      {0x31,"Catalyst"},
      {0x37,"AMIC"},
      {0x40,"SyncMOS"},
      {0x4A,"ESI"},
      {0x52,"Alliance Semiconductor"},
      {0x5E,"Tenx"},
      {0x62,"ON Semiconductor"},
      {0x62,"Sanyo"},
      {0x8C,"ESMT"},
      {0x89,"Intel"},
      {0x97,"Texas Instruments"},
      {0x9D,"PMC"},
      {0xAD,"Bright/Hyundai"},
      {0xB0,"Sharp"},
      {0xBF,"SST"},
      {0xC2,"Macronix"},
      {0xC8,"ELM"},
      {0xC8,"GigaDevice"},
      {0xDA,"Winbond"},
      {0xD5,"ISSI"},
      {0xD5,"Nantronics"},
      {0xEF,"Winbond"},
      {0xF8,"Fidelix"},
   };
   const int dim = sizeof (flash_list) / sizeof (struct flash_manufacturer);

   int i;
   
   for (i = 0; i < dim; i++)
   {
      if (flash_list[i].id == id)
      {
         strcpy (man, flash_list[i].man);
         break;
      }
   }
   
   if (i >= dim)
      strcpy (man, "Unknown");
   
   return;
}

void flash_read_id (struct ftdi_context *ftdi, struct spi_context *spi, byte *id)
{
   byte buf = RDID;

   spi_open (ftdi, spi);
   spi_write (ftdi, spi, &buf, 1);
   spi_read (ftdi, spi, id, 3);
   spi_close (ftdi, spi);
   
   return;
}


byte flash_read_status (struct ftdi_context *ftdi, struct spi_context *spi)
{
   byte buf = RDSR;
   byte flash_status;
   
   spi_open (ftdi, spi);
   spi_write (ftdi, spi, &buf, 1);
   spi_read (ftdi, spi, &flash_status, 1);
   spi_close (ftdi, spi);
   
   DEBUG_PRINT ("DEBUG: EEPROM status register:\n"
                 "    SRWD SEC  TB   BP[2:0]        WEL  BUSY\n"
                 BYTE_TO_BINARY_PATTERN"\n", 
                 BYTE_TO_BINARY (flash_status));
   
   return flash_status;
}

void flash_write_enable (struct ftdi_context *ftdi, struct spi_context *spi)
{
   byte buf = WREN;

   /* set WEL = 1 */
   spi_open (ftdi, spi);
   spi_write (ftdi, spi, &buf, 1);
   spi_close (ftdi, spi);
   
   return;
}

void flash_wait_if_busy (struct ftdi_context *ftdi, struct spi_context *spi)
{
   byte buf = RDSR;
   byte flash_status;
   
   spi_open (ftdi, spi);
   spi_write (ftdi, spi, &buf, 1);
   do
      spi_read (ftdi, spi, &flash_status, 1);
   while (flash_status & WIP);
   spi_close (ftdi, spi);
   
   return;
}


time_t time_sync (void)
{
   time_t t1, t2;
   
   do
   {
      time (&t1);
      do
      {
         time (&t2);
      } while (difftime (t2, t1) < 1);    /* exit if *at least* 1 sec has passed */
   } while (difftime (t2, t1) > 1);       /* exit if *no more than* 1 sec has passed */
   
   return t2;
}
