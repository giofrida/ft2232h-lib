/* Macros */
#define SETBIT(field, bit, val) (field) = (((field) & ~(1 << (bit))) | (((val) & 1) << (bit)))
#define GETBIT(field, bit)        (((field) & (1 << (bit))) >> (bit))

#ifdef DEBUG
#define DEBUG_PRINT(fmt, args...)    fprintf (stderr, fmt, ## args)
#else
#define DEBUG_PRINT(fmt, args...)
#endif


/* Commands definition */
#define CMD0    (0x40+0)    /* GO_IDLE_STATE */
#define CMD1    (0x40+1)    /* SEND_OP_COND (MMC) */
#define ACMD41  (0x40+41)   /* SEND_OP_COND (SDC) */
#define CMD8    (0x40+8)    /* SEND_IF_COND */
#define CMD9    (0x40+9)    /* SEND_CSD */
#define CMD10   (0x40+10)   /* SEND_CID */
#define CMD12   (0x40+12)   /* STOP_TRANSMISSION */
#define ACMD13  (0x40+13)   /* SD_STATUS (SDC) */
#define CMD16   (0x40+16)   /* SET_BLOCKLEN */
#define CMD17   (0x40+17)   /* READ_SINGLE_BLOCK */
#define CMD18   (0x40+18)   /* READ_MULTIPLE_BLOCK */
#define CMD23   (0x40+23)   /* SET_BLOCK_COUNT (MMC) */
#define ACMD23  (0x40+23)   /* SET_WR_BLK_ERASE_COUNT (SDC) */
#define CMD24   (0x40+24)   /* WRITE_BLOCK */
#define CMD25   (0x40+25)   /* WRITE_MULTIPLE_BLOCK */
#define CMD55   (0x40+55)   /* APP_CMD */
#define CMD58   (0x40+58)   /* READ_OCR */

/* Registers data structure */

/* R1 response */
#define IN_IDLE_STATE 0x01  /* In Idle State */
#define ERASE_RESET   0x02  /* Erase Reset */
#define ILLEGAL_CMD   0x04  /* Illegal Command */
#define CMD_CRC_ERR   0x08  /* Command CRC Error */
#define ERASE_SEQ_ERR 0x10  /* Erase Sequence Error */
#define ADDR_ERR      0x20  /* Address Error */
#define PARAM_ERR     0x40  /* Parameter Error */
#define R1_RSVD       0x80  /* reserved, must be 0 */

/* Error token */
#define ERR           0x01  /* Error */
#define CC_ERR        0x02  /* CC error */
#define ECC_FAIL      0x04  /* Card ECC failed */
#define OUT_OF_RANGE  0x08  /* Out of range */
#define CARD_LOCK     0x10  /* Card is locked */
#define TOK_RSVD      0xE0  /* reserved, must be 0 */

/* OCR register */
#define FULL_VDD_WINDOW  0x00FFFFFF /* Full VDD voltage window */
#define VDD_WINDOW       0x00FF8000 /* VDD (non-low-voltage) voltage window */
#define CCS              0x40000000 /* Card Capacity Status */
#define CARD_BUSY        0x80000000 /* Card power up status bit (busy) */

/* Type redefinition */
#ifndef FTDI_LIB_TYPES_DEFINED
typedef uint8_t  byte;
typedef uint16_t word;
typedef uint32_t dword;
#define FTDI_LIB_TYPES_DEFINED
#endif

struct sd_cid
{
   byte MID;
   char OID[3];
   char PNM[6];
   byte PRV;
   dword PSN;
   word MDT : 12;
   byte CRC : 7;
   
   byte raw[16];
};

struct sd_csd
{
   byte CSD_STRUCTURE      : 2;
   byte TAAC;
   byte NSAC;
   byte TRAN_SPEED;
   word CCC                : 12;
   byte READ_BL_LEN        : 4;
   byte READ_BL_PARTIAL    : 1;
   byte WRITE_BLK_MISALIGN : 1;
   byte READ_BLK_MISALIGN  : 1;
   byte DSR_IMP            : 1;
   dword C_SIZE            : 22; /* 22 bits to accomodate csd v2 */
   byte VDD_R_CURR_MIN     : 3;
   byte VDD_R_CURR_MAX     : 3;
   byte VDD_W_CURR_MIN     : 3;
   byte VDD_W_CURR_MAX     : 3;
   byte C_SIZE_MULT        : 3;
   byte ERASE_BLK_EN       : 1;
   byte SECTOR_SIZE        : 7;
   byte WP_GRP_SIZE        : 7;
   byte WP_GRP_ENABLE      : 1;
   byte R2W_FACTOR         : 3;
   byte WRITE_BL_LEN       : 4;
   byte WRITE_BL_PARTIAL   : 1;
   byte FILE_FORMAT_GRP    : 1;
   byte COPY               : 1;
   byte PERM_WRITE_PROTECT : 1;
   byte TMP_WRITE_PROTECT  : 1;
   byte FILE_FORMAT        : 2;
   byte CRC                : 7;
   
   byte raw[16];
};

struct sd_manufacturer
{
   byte mid;
   char man[64];
   char oid[3];
};

/* CID macros */
#define sd_cid_month(cid)     ((cid).MDT & 0x00F)
#define sd_cid_year(cid)      (2000 + (((cid).MDT & 0xFF0) >> 4))

/* CSD macros */
#define sd_csd_version(cid)   ((cid).CSD_STRUCTURE + 1)

static const double taac_timevalue[16] = {0, 1.0, 1.2, 1.3, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0, 7.0, 8.0};
static const int    taac_timemult[8]   = {1, 10, 100, 1, 10, 100, 1, 10};
static const char   taac_timeunit[3]   = {'n', 'u', 'm'};
#define sd_csd_data_access_time_unit(csd)      taac_timeunit[((csd).TAAC & 0b00000111) / 3]
#define sd_csd_data_access_time_value(csd)     (taac_timevalue[((csd).TAAC & 0b00111000) >> 3] * taac_timemult[(csd).TAAC & 0b00000111])
#define sd_csd_data_access_time_clock(csd)     ((csd).NSAC * 100)

static const double tran_timevalue[16] = {0, 1.0, 1.2, 1.3, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0, 5.5, 6.0, 7.0, 8.0};
static const int    tran_timemult[4]   = {100, 1, 10, 100};
#define sd_csd_transfer_speed_unit(csd)        ((((csd).TRAN_SPEED & 0b00000111) == 0) ? 'k' : 'M')
#define sd_csd_transfer_speed_value(csd)       (tran_timevalue[((csd).TRAN_SPEED & 0b000111000) >> 3] * tran_timemult[((csd).TRAN_SPEED & 0b00000111) & 0x03])

#define sd_csd_read_block_length(csd)          (((csd).READ_BL_LEN > 8 && (csd).READ_BL_LEN < 12) ? (1 << (csd).READ_BL_LEN) : 0)
#define sd_csd_write_block_length(csd)         (((csd).WRITE_BL_LEN > 8 && (csd).WRITE_BL_LEN < 12) ? (1 << (csd).WRITE_BL_LEN) : 0)

/* in bytes, must be normalized */
#define sd_csd_memory_capacity_value(csd)      (((csd).CSD_STRUCTURE == 0) ? ((csd).C_SIZE + 1) * (1 << ((csd).C_SIZE_MULT + 2)) * (1 << (csd).READ_BL_LEN) : ((csd).C_SIZE + 1) * 512 * 1024)
#define sd_csd_memory_capacity_normalize(mem)  ((mem > (1 << 30)) ? (mem / (1 << 30)) : (mem > (1 << 20)) ? (mem / (1 << 20)) : (mem > (1 << 10)) ? (mem / (1 << 10)) : mem)
#define sd_csd_memory_capacity_unit(mem)       ((mem > (1 << 30)) ? 'T' : (mem > (1 << 20)) ? 'M' : (mem > (1 << 10)) ? 'k' : '\0')
#define sd_csd_device_size_mult(csd)           (1 << (csd).C_SIZE_MULT)

static const double curr_min_value[8] = {0.5, 1, 5, 10, 25, 35, 60, 100};
static const double curr_max_value[8] = {1, 5, 10, 25, 35, 45, 80, 200};
#define sd_csd_current_min_read(csd)           curr_min_value [(csd).VDD_R_CURR_MIN]
#define sd_csd_current_min_write(csd)          curr_min_value [(csd).VDD_W_CURR_MIN]
#define sd_csd_current_max_read(csd)           curr_max_value [(csd).VDD_R_CURR_MAX]
#define sd_csd_current_max_write(csd)          curr_max_value [(csd).VDD_W_CURR_MAX]

#define sd_csd_sector_size(csd)                ((csd).SECTOR_SIZE + 1)   
#define sd_csd_wp_group_size(csd)              ((csd).WP_GRP_SIZE + 1)  

static const char file_type[5][128] = { "Hard disk-like file system with partition table", 
                                        "DOS FAT (floppy-like) with boot sector only (no partition table)",
                                        "Universal File Format",
                                        "Others/Unknown",
                                        "Reserved" };
#define sd_csd_file_format(csd)                (((csd).FILE_FORMAT_GRP == 0) ? file_type[(csd).FILE_FORMAT] : file_type[4])


void sd_init (struct ftdi_context *ftdi, struct spi_context *spi);
void sd_reset (struct ftdi_context *ftdi, struct spi_context *spi);
int sd_recognize (struct ftdi_context *ftdi, struct spi_context *spi);
time_t time_sync (void);

int sd_get_ocr (struct ftdi_context *ftdi, struct spi_context *spi, dword *ocr);
int sd_get_cid (struct ftdi_context *ftdi, struct spi_context *spi, struct sd_cid *cid);
int sd_get_csd (struct ftdi_context *ftdi, struct spi_context *spi, struct sd_csd *csd);
void sd_print_ocr_info (dword ocr);
void sd_print_cid_info (struct sd_cid cid);
void sd_print_csd_info (struct sd_csd csd);
void sd_cid_manufacturer (struct sd_cid cid, char *man);

int sd_send_command (struct ftdi_context *ftdi, struct spi_context *spi, byte *response, byte cmd, dword arg);
int sd_read_data (struct ftdi_context *ftdi, struct spi_context *spi, byte *data, int count);

int sd_is_r1_valid (byte r1);
int sd_is_token_valid (byte r1);
void interpret_r7_response (byte *response, byte *r1, dword *ocr);

/* Auxiliary functions */
byte crc_7 (byte *data, int count);
word crc_16 (byte *data, int count);
dword crc (byte *data, int count, dword poly);
void l_shift (dword *data, int count);

dword get_bits (byte *data, int length, int start_bit, int size);