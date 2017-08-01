/* SPI */
#define SCLK   0x01     /* Serial CloCK */
#define MOSI   0x02     /* Master Out Slave In */
#define MISO   0x04     /* Master In Slave Out */
#define CS     0x08     /* Chip Select */

#define MAX_INTERNAL_BUF_LENGTH 4096
#define MAX_SPI_BUF_LENGTH 65536

struct spi_context
{
   int CPOL;                        /* Clock POLarity (CPOL) */
   int CPHA;                        /* Clock PHAse (CPHA) */
   unsigned short int CDIV;         /* Clock DIVisor (CDIV) */
   int CDIV5;                       /* Clock DIVide by 5 (CDIV5) */
   
   int WRITE_LSB_FIRST;             
   int READ_LSB_FIRST;
   
   int MOSI_IDLE;                   /* Master Out Slave In IDLE level */
   int MISO_IDLE;                   /* Master In Slave Out IDLE level */    
};

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

/* for spi_write, spi_read functions */
#define HEADER_0  0
#define HEADER_1  1
#define HEADER_2  2
#define DATA      3

#define SPIMODE(spi)       ((spi).CPOL << 1 | (spi).CPHA)

struct spi_context spi_init (struct ftdi_context *ftdi,
                             int clock_idle, int clock_phase, unsigned short int clock_divisor, int clock_divide_by_5, int write_lsb_first, int read_lsb_first, int mosi_idle, int miso_idle);

void spi_open (struct ftdi_context *ftdi, struct spi_context spi);
void spi_close (struct ftdi_context *ftdi, struct spi_context spi);

void spi_write (struct ftdi_context *ftdi, struct spi_context spi, unsigned char *data, int data_length);
void spi_write_from_file (struct ftdi_context *ftdi, struct spi_context spi, FILE *fp, int data_length);
void spi_read (struct ftdi_context *ftdi, struct spi_context spi, unsigned char *data, int data_length);
void spi_read_to_file (struct ftdi_context *ftdi, struct spi_context spi, FILE *fp, int data_length);

void spi_print_clk_frequency (struct spi_context spi);