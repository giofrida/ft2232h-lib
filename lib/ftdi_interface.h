#define BAD_COMMAND 0xFA

#define GETBYTE(field, byte) (((field) >> (8 * (byte))) & 0xFF)

union ftdi_status
{
   struct 
   {
      unsigned int RSVD : 4;     // Reserved
      unsigned int CTS  : 1;     // Clear to send
      unsigned int DTS  : 1;     // Data set ready
      unsigned int RI   : 1;     // Ring indicator
      unsigned int RLSD : 1;     // Carrier detect
      
      unsigned int DR   : 1;     // Data ready
      unsigned int OE   : 1;     // Overrun error
      unsigned int PE   : 1;     // Parity error
      unsigned int FE   : 1;     // Framing error
      unsigned int BI   : 1;     // Break interrupt
      unsigned int THRE : 1;     // Transmitter holding register
      unsigned int TEMT : 1;     // Transmitter buffer empty
      unsigned int RCVR : 1;     // Error in RCVR FIFO
   };

   unsigned short int value;
};

int ftdi_write_data_and_check (struct ftdi_context *ftdi, char *buf, int size);
struct ftdi_context *ftdi_open (struct ftdi_context *ftdi);
void ftdi_close (struct ftdi_context *ftdi);
void ftdi_exit (struct ftdi_context *ftdi, char *error_string, int error_code);

int ftdi_wait_and_read_data (struct ftdi_context *ftdi, unsigned char *buf, int size);
int ftdi_wait_and_write_data (struct ftdi_context *ftdi, unsigned char *buf, int size);

int is_tx_buffer_empty (struct ftdi_context *ftdi, union ftdi_status *status);
int is_tx_error (struct ftdi_context *ftdi, union ftdi_status *status);