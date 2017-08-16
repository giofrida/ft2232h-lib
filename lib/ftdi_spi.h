/** 
   @defgroup MPSSE_PINS_GRP MPSSE pins bitmasks
   @{ 
*/
#define SCLK   0x01     /**< Serial CloCK */
#define MOSI   0x02     /**< Master Out Slave In */
#define MISO   0x04     /**< Master In Slave Out */
#define CS     0x08     /**< Chip Select */
/**@} */

/**
   @defgroup BUF_LENGTH
   @{ 
*/
#define MAX_INTERNAL_BUF_LENGTH 4096
#define MAX_SPI_BUF_LENGTH 65536
/**@} */

/**
   @defgroup SPI_DEFS
   @{ 
*/
#define HEADER_0  0
#define HEADER_1  1
#define HEADER_2  2
#define DATA      3
/**@} */

/**
   @defgroup BYTE_DEFS
   @{ 
*/
#define SETBYTE(field, byte, val) (field) = ((field) & ~(0xFF << 8 * (byte))) | (((val) & 0xFF) << 8 * (byte))
#define GETBYTE(field, byte) (((field) & (0xFF << 8 * (byte))) >> 8 * (byte))
/**@} */


#define SPIMODE(spi)       ((*spi).CPOL << 1 | (*spi).CPHA)     /**< Builds SPI mode value from CPOL and CPHA */
#define spi_free(spi)      free(spi)                            /**< De-allocates spi_context structure */

#ifndef FTDI_LIB_TYPES_DEFINED
typedef uint8_t  byte;  /**< 8-bit unsigned integer type */
typedef uint16_t word;  /**< 16-bit unsigned integer type */
typedef uint32_t dword; /**< 32-bit unsigned integer type */
#define FTDI_LIB_TYPES_DEFINED
#endif

/* SPI Modes chart
CPOL: 0 if CLK=0 when idle, 1 if CLK=1 when idle
CPHA: if CPOL=0
         0 if data is clocked out on the falling edge of SCLK and read on the rising edge of SCLK
         1 if data is clocked out on the rising edge of SCLK and read on the falling edge of SCLK
      if CPOL=1
         0 if data is clocked out on the rising edge of SCLK and read on the falling edge of SCLK
         1 if data is clocked out on the falling edge of SCLK and read on the rising edge of SCLK
    
CDIV5: 0 if SCLK frequency is not divided by 5, 1 if SCLK if divided by 5    
CDIV: SCLK divisor
*/

struct bits
{
   byte level;
   byte io;
};

struct spi_context
{
   int CPOL;                        /**< Clock POLarity (CPOL) */
   int CPHA;                        /**< Clock PHAse (CPHA) */
   word CDIV;                       /**< Clock DIVisor (CDIV) */
   int CDIV5;                       /**< Clock DIVide by 5 (CDIV5) */
   
   int WRITE_LSB_FIRST;             /**< Write LSB first */
   int READ_LSB_FIRST;              /**< Read LSB first */
   
   int MOSI_IDLE;                   /**< Master Out Slave In IDLE level */
   
   int LOOPBACK_ON;                 /**< Internal Loopback enable bit */
   
   /* port levels, i/o direction */
   struct bits low_bits;
   struct bits high_bits;
};


struct spi_context *spi_init (struct ftdi_context *ftdi,
                             int clock_idle, int clock_phase, word clock_divisor, int clock_divide_by_5, int mosi_idle, int write_lsb_first, int read_lsb_first, int loopback_on);

void spi_open (struct ftdi_context *ftdi, struct spi_context *spi);
void spi_close (struct ftdi_context *ftdi, struct spi_context *spi);

void spi_write (struct ftdi_context *ftdi, struct spi_context *spi, byte *data, int size);
void spi_read (struct ftdi_context *ftdi, struct spi_context *spi, byte *data, int size);
int spi_write_from_file (struct ftdi_context *ftdi, struct spi_context *spi, FILE *fp, int size);
int spi_read_to_file (struct ftdi_context *ftdi, struct spi_context *spi, FILE *fp, int size);

void spi_print_clk_frequency (struct spi_context *spi);
double spi_frequency (struct spi_context *spi);
double spi_clk_period (struct spi_context *spi);

void ftdi_set_bits_low (struct ftdi_context *ftdi, struct spi_context *spi, byte mask, byte level, byte io);
void ftdi_set_bits_high (struct ftdi_context *ftdi, struct spi_context *spi, byte mask, byte level, byte io);
byte ftdi_get_bits_low (struct ftdi_context *ftdi, struct spi_context *spi);
byte ftdi_get_bits_high (struct ftdi_context *ftdi, struct spi_context *spi);