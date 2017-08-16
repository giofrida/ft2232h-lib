#define FTDI_BAD_COMMAND 0xFA    /**< FTDI device returns 0xFA if a bad command is received */

/**< Auxiliary macro used to print debug information when program is compiled with DEBUG symbol defined */
#ifdef DEBUG
#define DEBUG_PRINT(fmt, args...)    fprintf (stderr, fmt, ## args)
#else
#define DEBUG_PRINT(fmt, args...)
#endif

/** 
   @defgroup MODEM_STAT_GRP Modem status register
   @{ 
*/
/* first 4 bits of first byte must be 0 */
#define CTS  0x1000     /**< Clear To Send */
#define DSR  0x2000     /**< Data Set Ready */
#define RI   0x4000     /**< Ring Indicator */
#define RLSD 0x8000     /**< Recieve Line Signal Detect */

#define DR   0x0001     /**< Data Ready */
#define OE   0x0002     /**< Overrun Error */
#define PE   0x0004     /**< Parity Error */
#define FE   0x0008     /**< Framing Error */
#define BI   0x0010     /**< Break Interrupt */
#define THRE 0x0020     /**< Transmitter Holding REgister */
#define TEMT 0x0040     /**< Transmitter buffer EMpTy */
#define RCVR 0x0080     /**< Error in Receiver FIFO */
/**@} */

#ifndef FTDI_LIB_TYPES_DEFINED
typedef uint8_t  byte;  /**< 8-bit unsigned integer type */
typedef uint16_t word;  /**< 16-bit unsigned integer type */
typedef uint32_t dword; /**< 32-bit unsigned integer type */
#define FTDI_LIB_TYPES_DEFINED
#endif

struct ftdi_context *ftdi_open (void);
void ftdi_close (struct ftdi_context *ftdi);
void ftdi_exit (struct ftdi_context *ftdi, char *error_string, int error_code);

int ftdi_write_data_and_check (struct ftdi_context *ftdi, byte *data, int size);
int ftdi_read_data_and_wait (struct ftdi_context *ftdi, byte *data, int size);
int ftdi_write_data_and_wait (struct ftdi_context *ftdi, byte *data, int size);

int ftdi_tx_buf_empty (struct ftdi_context *ftdi, word *status);
int ftdi_tx_error (struct ftdi_context *ftdi, word *status);