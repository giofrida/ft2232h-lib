#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <libftdi1/ftdi.h>

#include "ftdi_interface.h"
#include "ftdi_spi.h"
#include "sd_spi.h"

void sd_init (struct ftdi_context *ftdi, struct spi_context *spi)
{ 
   byte buf[3];
   int ret;
   
   /* ensure CS#, MOSI is held high */
   ftdi_set_bits_low (ftdi, spi, MOSI|CS, MOSI|CS, MOSI|CS);

   /* wait at least 1ms to let vdd to reach vdd_min level */
   usleep (1000);

   /* send at least 74 clock cycles within 1ms */
   buf[0] = CLK_BYTES;
   buf[1] = 10-1;    /* 10*8=80 clock cycles */
   buf[2] = 0;
   if ((ret = ftdi_write_data (ftdi, buf, 3)) < 0)
      ftdi_exit (ftdi, "ERROR: Unable to init SD card: %d (%s)\n", ret);
  
   return;
}

void sd_reset (struct ftdi_context *ftdi, struct spi_context *spi)
{
   byte r1;
   int ret;
   
   /* send CMD0 (GO_IDLE_STATE) command to reset sd card */
   /* send until card does not respond */
   while ((ret = sd_send_command (ftdi, spi, &r1, CMD0, 0x00000000)) < 0);
   
   /* if not in idle state or an error occured */
   if (!(r1 & IN_IDLE_STATE) || ret <= 0) 
   {
      /* terminate program */
      if (ret < 0)
         fprintf (stderr, "ERROR: Unable to receive a valid response from SD card\n");
      else
         fprintf (stderr, "ERROR: Unexpected SD card response: 0x%.2X\n", r1);
      
      spi_free (spi);
      ftdi_free (ftdi);
      exit (EXIT_FAILURE);
   }

   return;
}

int sd_recognize (struct ftdi_context *ftdi, struct spi_context *spi)
{
   byte buf[5];
   int ret;
   byte r1;
   dword ocr;
   
   time_t start, end; 

   /* send CMD8 (SEND_IF_COND) command to recognize sd card version */
   ret = sd_send_command (ftdi, spi, buf, CMD8, 0x000001AA);

   /* split response packet */
   interpret_r7_response (buf, &r1, &ocr);

   /* if error or no response */
   if (ret <= 0)
   {
      /* send ACMD41 with 0x00000000 as argument */
      start = time_sync ();
      do
      {
         if ((ret = sd_send_command (ftdi, spi, &r1, CMD55, 0x00000000)) > 0)
            ret = sd_send_command (ftdi, spi, &r1, ACMD41, 0x00000000);

         time (&end);
      /* exit if error or timeout occured or r1 is not equal to 0x01 (in idle state) */
      } while (difftime (end, start) < 1 && ret > 0 && (r1 == IN_IDLE_STATE));

      if (r1 == 0x00)
      {
         /* SD version 1 */
         return 1;
      }
      /* else */
      
      start = time_sync ();
      do
      {
         ret = sd_send_command (ftdi, spi, &r1, CMD1, 0x00000000);
         
         time (&end);
      /* exit if error or timeout occured or r1 is not equal to 0x01 (in idle state) */
      } while (difftime (end, start) < 1 && ret > 0 && (r1 == IN_IDLE_STATE));
      
      if (r1 == 0x00)
      {
         /* MMC version 3 */
         return 0;
      }
   } 
   else if (ocr == 0x000001AA)
   {  
      /* send ACMD41 with 0x40000000 as argument */
      start = time_sync ();
      do
      {
         if ((ret = sd_send_command (ftdi, spi, &r1, CMD55, 0x00000000)) > 0)
            ret = sd_send_command (ftdi, spi, &r1, ACMD41, 0x40000000);
         
         time (&end);
      /* exit if error or timeout occured or r1 is not equal to 0x01 (in idle state) */
      } while (difftime (end, start) < 1 && ret > 0 && (r1 == IN_IDLE_STATE));

      if (r1 == 0x00)
      {
         /* SD version 2 */
         
         /* read ocr register */
         sd_send_command (ftdi, spi, buf, CMD58, 0x00000000);

         /* split response packet */
         interpret_r7_response (buf, &r1, &ocr);
         
         if (ocr & CCS)
         {
            /* byte address */
            
            /* force block size to 512 bytes to work with FAT file system */
            sd_send_command (ftdi, spi, &r1, CMD16, 0x00000200);

            return 3;
         }
         else
         {
            /* block address */
            return 2;
         }
      }
   }
   /* else (if ocr != 0x000001AA) */
   
   /* unknown card, terminate */
   fprintf (stderr, "ERROR: Unknown SD card\n");
   spi_free (spi);
   ftdi_free (ftdi);
   exit (EXIT_FAILURE);
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

int sd_get_ocr (struct ftdi_context *ftdi, struct spi_context *spi, dword *ocr)
{
   byte buf[5];
   byte r1;
   int ret;
   
   /* read ocr register */
   if ((ret = sd_send_command (ftdi, spi, buf, CMD58, 0x00000000)) <= 0)
   {
      /* error or no response */
      fprintf (stderr, "ERROR: Could not retrieve OCR register value\n");
      return -1;
   }

   /* split response packet */
   interpret_r7_response (buf, &r1, ocr);
   
   printf ("INFO: SD OCR register: %.8X\n", *ocr);   
   
   return 1;
}

int sd_get_cid (struct ftdi_context *ftdi, struct spi_context *spi, struct sd_cid *cid)
{
   byte buf[16];
   byte r1;
   int i, ret;
   
   /* send cid register */
   if ((ret = sd_send_command (ftdi, spi, &r1, CMD10, 0x00000000)) <= 0)
   {
      /* error or no response */
      fprintf (stderr, "ERROR: Could not retrieve CID register value\n");
      return -1;
   }
   
   if ((ret = sd_read_data (ftdi, spi, buf, 16)) <= 0)
   {
      /* error or no response */
      fprintf (stderr, "ERROR: Could not retrieve CID register value\n");
      return -1;
   }
   
   if ((crc_7 (buf, 15) << 1 | 0x01) != buf[15])
   {
      fprintf (stderr, "ERROR: CRC in CID is incorrect!");
      return 0;
   }
   
   printf ("INFO: SD CID register: ");
   for (i = 0; i < 16; i++)
      printf ("%.2X", buf[i]);
   printf ("\n");
   
   memcpy (cid->raw, buf, 16);
   
   /* write structure */
   cid->MID    = get_bits (buf, 16, 120, 8);
   cid->OID[0] = get_bits (buf, 16, 112, 8);
   cid->OID[1] = get_bits (buf, 16, 104, 8);
   cid->OID[2] = '\0';
   cid->PNM[0] = get_bits (buf, 16, 96, 8);
   cid->PNM[1] = get_bits (buf, 16, 80, 8);
   cid->PNM[2] = get_bits (buf, 16, 72, 8);
   cid->PNM[3] = get_bits (buf, 16, 64, 8);
   cid->PNM[4] = '\0';
   cid->PRV    = get_bits (buf, 16, 56, 8);
   cid->PSN    = get_bits (buf, 16, 24, 32);
   cid->MDT    = get_bits (buf, 16, 8, 12);
   cid->CRC    = get_bits (buf, 16, 1, 7);
   
   return 1;
}

int sd_get_csd (struct ftdi_context *ftdi, struct spi_context *spi, struct sd_csd *csd)
{
   byte buf[16];
   byte r1;
   int i, ret;
   
   /* send csd register */
   if ((ret = sd_send_command (ftdi, spi, &r1, CMD9, 0x00000000)) <= 0)
   {
      /* error or no response */
      fprintf (stderr, "ERROR: Could not retrieve CSD register value\n");
      return -1;
   }
   
   if ((ret = sd_read_data (ftdi, spi, buf, 16)) <= 0)
   {
      /* error or no response */
      fprintf (stderr, "ERROR: Could not retrieve CSD register value\n");
      return -1;
   }
   
   if ((crc_7 (buf, 15) << 1 | 0x01) != buf[15])
   {
      fprintf (stderr, "ERROR: CRC in CSD is incorrect!");
      return 0;
   }
   
   printf ("INFO: SD CSD register: ");
   for (i = 0; i < 16; i++)
      printf ("%.2X", buf[i]);
   printf ("\n");
   
   memcpy (csd->raw, buf, 16);
   
   csd->CSD_STRUCTURE      = get_bits (buf, 16, 126, 2);
   csd->TAAC               = get_bits (buf, 16, 112, 8);
   csd->NSAC               = get_bits (buf, 16, 104, 8);
   csd->TRAN_SPEED         = get_bits (buf, 16, 96, 8);
   csd->CCC                = get_bits (buf, 16, 84, 12);
   csd->READ_BL_LEN        = get_bits (buf, 16, 80, 4);
   csd->READ_BL_PARTIAL    = get_bits (buf, 16, 79, 1);
   csd->WRITE_BLK_MISALIGN = get_bits (buf, 16, 78, 1);
   csd->READ_BLK_MISALIGN  = get_bits (buf, 16, 77, 1);
   csd->DSR_IMP            = get_bits (buf, 16, 76, 1);
   if (csd->CSD_STRUCTURE == 0)
   {
      csd->C_SIZE          = get_bits (buf, 16, 62, 12);
      csd->VDD_R_CURR_MIN  = get_bits (buf, 16, 59, 3);
      csd->VDD_R_CURR_MAX  = get_bits (buf, 16, 56, 3);
      csd->VDD_W_CURR_MIN  = get_bits (buf, 16, 53, 3);
      csd->VDD_W_CURR_MAX  = get_bits (buf, 16, 50, 3);
      csd->C_SIZE_MULT     = get_bits (buf, 16, 47, 3);
   }
   else
   {
      csd->C_SIZE          = get_bits (buf, 16, 48, 22);
   }
   csd->ERASE_BLK_EN       = get_bits (buf, 16, 46, 1);
   csd->SECTOR_SIZE        = get_bits (buf, 16, 39, 7);
   csd->WP_GRP_SIZE        = get_bits (buf, 16, 32, 7);
   csd->WP_GRP_ENABLE      = get_bits (buf, 16, 31, 1);
   csd->R2W_FACTOR         = get_bits (buf, 16, 26, 3);
   csd->WRITE_BL_LEN       = get_bits (buf, 16, 22, 4);
   csd->WRITE_BL_PARTIAL   = get_bits (buf, 16, 21, 1);
   csd->FILE_FORMAT_GRP    = get_bits (buf, 16, 15, 1);
   csd->COPY               = get_bits (buf, 16, 14, 1);
   csd->PERM_WRITE_PROTECT = get_bits (buf, 16, 13, 1);
   csd->TMP_WRITE_PROTECT  = get_bits (buf, 16, 12, 1);
   csd->FILE_FORMAT        = get_bits (buf, 16, 11, 2);
   csd->CRC                = get_bits (buf, 16, 1, 7);
   
   return 1;
}

void sd_print_ocr_info (dword ocr)
{
   int i, min, max;
   double vdd_min, vdd_max;

   /* get minimum and maximum working voltage */
   min = 23;
   max = 15;
   
   for (i = 15; i < 24; i++)
   {
      if (ocr & (1 << i))
      {
         if (i < min) min = i;
         if (i > max) max = i;
      }
   }
   
   vdd_min = 2.6 + 0.1 * (min - 14);
   vdd_max = 2.7 + 0.1 * (max - 14);
   
   printf ("INFO: SD card VDD range: %.1fV-%.1fV\n", vdd_min, vdd_max);
   
   return; 
}

void sd_print_cid_info (struct sd_cid cid)
{
   char manufacturer[64];
   
   /* elaborate data */
   sd_cid_manufacturer (cid, manufacturer);
   
   printf ("         Manufacturer ID (MID): 0x%.2X (%s)\n", cid.MID, manufacturer);
   printf ("      OEM/Application ID (OID): 0x%.2X%.2X (%s)\n", cid.OID[0], cid.OID[1], cid.OID);
   printf ("            Product name (PNM): %s\n", cid.PNM);
   printf ("        Product revision (PRV): 0x%.2X (%d.%d)\n", cid.PRV, cid.PRV >> 4, cid.PRV & 0x0F);
   printf ("   Product serial number (PSN): 0x%.8X\n", cid.PSN);
   printf ("      Manufacturing date (MDT): 0x%.3X (%d/%d) \n", cid.MDT, sd_cid_month (cid), sd_cid_year (cid));
   printf ("           CRC7 checksum (CRC): 0x%.2X\n", cid.CRC);
   
   return;
}

void sd_print_csd_info (struct sd_csd csd)
{
   int i;
   double mem_capacity;
   
   mem_capacity = sd_csd_memory_capacity_value (csd);
   
   printf ("                                       CSD structure: 0x%.2X (v%d.0)\n", csd.CSD_STRUCTURE, sd_csd_version (csd));
   printf ("                      Data read access time 1 (TAAC): 0x%.2X (%.1f %cs)\n", csd.TAAC, sd_csd_data_access_time_value (csd), sd_csd_data_access_time_unit (csd));
   printf ("        Data read access time 2 in CLK cycles (NSAC): 0x%.2X (%d clock cycles)\n", csd.NSAC, sd_csd_data_access_time_clock (csd));
   printf ("                 Max data transfer rate (TRAN_SPEED): 0x%.2X (%.1f %cbit/s)\n", csd.TRAN_SPEED, sd_csd_transfer_speed_value (csd), sd_csd_transfer_speed_unit (csd));
   printf ("                            Card Command Class (CCC): 0x%.3X (classes: ", csd.CCC);
   for (i = 0; i < 11; i++)
      if (GETBIT (csd.CCC, i))
         printf ("%d ", i);
   printf ("\b)\n");
   printf ("            Max read data block length (READ_BL_LEN): 0x%.1X (%d bytes)\n", csd.READ_BL_LEN, sd_csd_read_block_length (csd));
   printf ("   Partial blocks for read allowed (READ_BL_PARTIAL): %d\n", csd.READ_BL_PARTIAL);
   printf ("       Write block misalignment (WRITE_BLK_MISALIGN): %d\n", csd.WRITE_BLK_MISALIGN);
   printf ("        Read block misalignment (WRITE_BLK_MISALIGN): %d\n", csd.WRITE_BLK_MISALIGN);
   printf ("                           DSR implemented (DSR_IMP): %d\n", csd.DSR_IMP);
   if (csd.CSD_STRUCTURE == 0)
   {
      printf ("                                Device size (C_SIZE): 0x%.3X (%.1f %cbytes)\n", csd.C_SIZE, sd_csd_memory_capacity_normalize (mem_capacity), sd_csd_memory_capacity_unit (mem_capacity));
      printf ("          Max read current @VDD min (VDD_R_CURR_MIN): 0x%.1X (%.1f mA)\n", csd.VDD_R_CURR_MIN, sd_csd_current_min_read (csd));
      printf ("          Max read current @VDD max (VDD_R_CURR_MAX): 0x%.1X (%.1f mA)\n", csd.VDD_R_CURR_MAX, sd_csd_current_max_read (csd));
      printf ("         Max write current @VDD min (VDD_W_CURR_MIN): 0x%.1X (%.1f mA)\n", csd.VDD_W_CURR_MIN, sd_csd_current_min_write (csd));
      printf ("         Max write current @VDD max (VDD_W_CURR_MAX): 0x%.1X (%.1f mA)\n", csd.VDD_W_CURR_MAX, sd_csd_current_max_write (csd));
      printf ("                Device size multiplier (C_SIZE_MULT): 0x%.1X (%d Bytes)\n", csd.C_SIZE_MULT, sd_csd_device_size_mult (csd));
   }
   else
   {
      printf ("                                Device size (C_SIZE): 0x%.6X (%.1f %cbytes)\n", csd.C_SIZE, sd_csd_memory_capacity_normalize (mem_capacity), sd_csd_memory_capacity_unit (mem_capacity));
   }
   printf ("            Erase single block enable (ERASE_BLK_EN): %d\n", csd.ERASE_BLK_EN);
   printf ("                     Erase sector size (SECTOR_SIZE): 0x%.1X (%d blocks)\n", csd.SECTOR_SIZE, sd_csd_sector_size (csd));
   printf ("              Write protect group size (WP_GRP_SIZE): 0x%.1X (%d blocks)\n", csd.WP_GRP_SIZE, sd_csd_wp_group_size (csd));
   printf ("          Write protect group enable (WP_GRP_ENABLE): %d\n", csd.WP_GRP_ENABLE);
   printf ("                     Write speed factor (R2W_FACTOR): %d\n", csd.R2W_FACTOR);
   printf ("          Max write data block length (WRITE_BL_LEN): 0x%.1X (%d bytes)\n", csd.WRITE_BL_LEN, sd_csd_write_block_length (csd));
   printf (" Partial blocks for write allowed (WRITE_BL_PARTIAL): %d\n", csd.WRITE_BL_PARTIAL);
   printf ("                 File format group (FILE_FORMAT_GRP): %d\n", csd.FILE_FORMAT_GRP);
   printf ("                              Copy flag (OTP) (COPY): %d\n", csd.COPY);
   printf ("     Permanent write protection (PERM_WRITE_PROTECT): %d\n", csd.PERM_WRITE_PROTECT);
   printf ("      Temporary write protection (TMP_WRITE_PROTECT): %d\n", csd.TMP_WRITE_PROTECT);
   printf ("                           File format (FILE_FORMAT): %d (%s)\n", csd.FILE_FORMAT, sd_csd_file_format (csd));
   printf ("                                           CRC (CRC): 0x%.2X\n", csd.CRC);
   
   return;
}

void sd_cid_manufacturer (struct sd_cid cid, char *man)
{
   const struct sd_manufacturer sd_list[] = {
      {0x01,"Panasonic","PA"},
      {0x02,"Toshiba","TM"},
      {0x03,"Sandisk","SD"},
      {0x13,"KingMax","HG"},
      {0x13,"KingMax","KG"},
      {0x16,"Matrix",""},
      {0x1B,"Samsung","SM"},
      {0x27,"Phison","PH"},
      {0x30,"Sandisk","SD"},
      {0x41,"Kingston","42"},
      {0x5D,"swissbit","SB"},
   };
   const int dim = sizeof (sd_list) / sizeof (struct sd_manufacturer);
   
   int i;
   
   for (i = 0; i < dim; i++)
   {
      if (sd_list[i].mid == cid.MID && !strcmp (sd_list[i].oid, cid.OID))
      {
         strcpy (man, sd_list[i].man);
         break;
      }
   }
   
   if (i >= dim)
      strcpy (man, "Unknown");
   
   return;
}


int sd_send_command (struct ftdi_context *ftdi, struct spi_context *spi, byte *response, byte cmd, dword arg)
{
   byte pkt[6], temp;
   int i, count, timeout, processing;
   
   /* wait for card to be ready to receive a new command */
   do
      spi_read (ftdi, spi, &temp, 1);
   while (temp != 0xFF);

   /* prepare sd packet */
   if (GETBIT (cmd, 7) == 1)
      printf ("WARNING: Command 0x%.2X has first bit set to 1!\n", cmd);
   
   pkt[0] = cmd;
   pkt[1] = GETBYTE (arg, 3);
   pkt[2] = GETBYTE (arg, 2);
   pkt[3] = GETBYTE (arg, 1);
   pkt[4] = GETBYTE (arg, 0);   
   pkt[5] = (crc_7 (pkt, 5) << 1) | 0x01;    /* crc must be left shifted and first bit must be 1 */
   
   /* send packet */
   spi_write (ftdi, spi, pkt, 6);
   
   DEBUG_PRINT ("DEBUG: Sending command ");
   for (i = 0; i < 6; i++)
      DEBUG_PRINT (" 0x%.2X", pkt[i]);
   DEBUG_PRINT ("\nDEBUG: Command response");
   
   /* read response */
   ftdi_set_bits_low (ftdi, spi, MOSI, MOSI, MOSI);   /* ensure MOSI is held high during read */
   
   timeout = 0;
   i = 0;
   processing = 1;
   count = (cmd == CMD8 || cmd == CMD58) ? 5 : 1;     /* CMD8 and CMD58 returns a R7/R3 response, which is 5 bytes long */
   
   do
   {
      spi_read (ftdi, spi, response + i, 1);
      DEBUG_PRINT (" 0x%.2X", response[i]);
      
      /* if no data has been received */
      if (response[i] == 0xFF && processing)
      {
         timeout++;     /* sd card is still processing */
      }
      else
      {
         processing = 0;
         i++;           /* get new data */
      }
   }
   while (i < count && timeout < 8);
  
   DEBUG_PRINT ("\n");
   
   if (timeout >= 8)
      return -1;
   
   /* response received */
   if (sd_is_r1_valid (response[0]))
      return 0;
   
   return 1;
}

int sd_read_data (struct ftdi_context *ftdi, struct spi_context *spi, byte *data, int count)
{
   byte token, crc[2];
   int timeout, processing, i;
   
   timeout = 0;
   processing = 1;

   DEBUG_PRINT ("DEBUG: Receiving data  ");
   /* read data/error token */
   do
   {
      spi_read (ftdi, spi, &token, 1);
      DEBUG_PRINT (" 0x%.2X", token);
      
      /* if no data has been received */
      if (token == 0xFF && processing)
         timeout++;     /* sd card is still processing */
      else
         processing = 0;
      
   } while (processing && timeout < 8);
   
   if (timeout >= 8)
   {
      DEBUG_PRINT ("\n");
      return -1;
   }
   
   if (!sd_is_token_valid (token))
   {
      DEBUG_PRINT ("\n");
      return 0;
   }
   
   /* read actual data */
   spi_read (ftdi, spi, data, count);
   for (i = 0; i < count; i++)
      DEBUG_PRINT (" 0x%.2X", data[i]);
   
   /* read crc */
   spi_read (ftdi, spi, crc, 2);
   DEBUG_PRINT (" 0x%.2X 0x%.2X\n\n", crc[1], crc[2]);
   
   /* check block using crc */
   if (crc_16 (data, count) != get_bits (crc, 2, 0, 16))
   {
      fprintf (stderr, "ERROR: CRC in data block is incorrect!\n");
      return 0;
   }

   return 1;
}


int sd_is_token_valid (byte token)
{
   if (token == 0xFE || token == 0xFC || token == 0xF1)
      /* it's a data token */
      return 1;
   
   if (!(token & TOK_RSVD))
   {
      /* it's an error token */
      printf ("WARNING: SD card responded with an error token, 0x%.2X received:\n", token);
      
      if (token & ERR)           printf ("   - Error\n");
      if (token & CC_ERR)        printf ("   - CC Error\n");
      if (token & ECC_FAIL)      printf ("   - Card ECC Failed\n");
      if (token & OUT_OF_RANGE)  printf ("   - Out Of Range\n");
      if (token & CARD_LOCK)     printf ("   - Card is Locked\n");
      
      return 0;
   }
   else
   {
      fprintf (stderr, "ERROR: SD card token not valid, 0x%.2X received\n", token);
      return -1;
   }
}

int sd_is_r1_valid (byte r1)
{
   /* Checks if sd card has detected an error */
   
   /* if all the first six bits are set to 0, no error has been detected */
   if (!(r1 & 0xFC))
      return 0;
   
   printf ("WARNING: SD card response not valid, 0x%.2X received:\n", r1);
   
   if (r1 & R1_RSVD)       printf ("   - reserved bit is not 0\n");
   if (r1 & PARAM_ERR)     printf ("   - Parameter Error\n");
   if (r1 & ADDR_ERR)      printf ("   - Address Error\n");
   if (r1 & ERASE_SEQ_ERR) printf ("   - Erase Sequence Error\n");
   if (r1 & CMD_CRC_ERR)   printf ("   - Command CRC Error\n");
   if (r1 & ILLEGAL_CMD)   printf ("   - Illegal Command\n");
   
   return 1;
}

void interpret_r7_response (byte *response, byte *r1, dword *ocr)
{
   *r1 = response[0];
   *ocr = get_bits (&response[1], 4, 0, 32);

   return;
}

byte crc_7 (byte *data, int count)
{
   /* G(x)=x^7+x^3+1*/
   const dword poly = (1 << 7) + (1 << 3) + 1;
   return (byte)crc (data, count, poly);
}

word crc_16 (byte *data, int count)
{
   /* G(x)=x^16+x^12+x^5+1 */
   const dword poly = (1 << 16) + (1 << 12) + (1 << 5) + 1;
   return (word)crc (data, count, poly);
}

dword crc (byte *data, int count, dword poly)
{
   const int BITS_DWORD = 8 * sizeof (dword);
   const int bits = count * 8 * sizeof (byte);
   
   int i, count_dword;              /* other */
   int start_bit, cur_length;       /* data preparation */
   int digits;                      /* division */
   
   dword *array;
   dword crc;
   
   /* get maximum power of 2 in polynomial */
   i = BITS_DWORD - 1;
   while (GETBIT (poly, i) == 0)
      i--;

   const int POLY_BITS = i + 1;
   const int TOTAL_BITS = bits + i;

   /* prepare dword array for M(x) * x^k */  
   count_dword = TOTAL_BITS / BITS_DWORD;
   if (TOTAL_BITS % BITS_DWORD)
      count_dword += 1;

   array = (dword *)malloc (sizeof (dword) * count_dword);    
   memset (array, 0, sizeof (dword) * count_dword);           /* init to 0 */
   
   /* copy bytes in dword array */
   start_bit = 0;
   i = count_dword - 1;
   while (start_bit < bits)
   {
      if ((cur_length = (bits - start_bit)) >= BITS_DWORD)
         cur_length = BITS_DWORD;

      array[i] = get_bits (data, count, start_bit, cur_length);
      start_bit += cur_length;
      i--;
   }

   /* do M(x) * x^k */
   for (i = 0; i < (POLY_BITS - 1); i++)
      l_shift (array, count_dword);
   
   /* prepare for division */
   
   /* align polynomial */
   while (GETBIT (poly, (BITS_DWORD - 1)) == 0)
      poly <<= 1;

   
   /* init to number of total bits in dword representation */
   digits = BITS_DWORD * count_dword;                
   do 
   {
      /* subtract polynomial if possible */
      if (GETBIT (array[0], (BITS_DWORD - 1)) != 0)
         array[0] ^= poly;
      
      /* align number */
      l_shift (array, count_dword);
      digits--;
   } while (digits >= POLY_BITS);               /* exit if message cannot not be divided anymore */
  
   crc = array[0] >> (BITS_DWORD - (POLY_BITS - 1));
   free (array);
   
   return crc;
}

void l_shift (dword *data, int count)
{
   int i;
   
   /* bitwise right shift array of 1 position */
   for (i = 0; i < count - 1; i++)
   {
      data[i] <<= 1;
      data[i] |= (data[i + 1] >> ((8 * sizeof (dword)) - 1));
   }
   
   data[count - 1] <<= 1;
   
   return;
}

dword get_bits (byte *data, int length, int start_bit, int size)
{
   int i, bit, last_bit, bit_in_dword, bit_in_byte;
   dword res = 0x00000000;

   last_bit = start_bit + size;

   bit_in_dword = 0;
   for (bit = start_bit; bit < last_bit; bit++)
   {
      i = (length - 1) - bit / (8 * sizeof (byte));
      bit_in_byte = bit % (8 * sizeof (byte));
      
      SETBIT (res, bit_in_dword, GETBIT (data[i], bit_in_byte));

      bit_in_dword++;
   }
   
   return res;
}