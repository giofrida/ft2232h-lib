#define WREN   0x06     /* Write enable */
/* Sets the write enable latch bit (WEL) */
#define WRDI   0x04     /* Write disable */
/* Resets the write enable latch bit (WEL) */

#define RDSR   0x05     /* Read status register */
/* Reads out the status register */
#define WRSR   0x01     /* Write status register */
/* Writes new values to the status register */

#define READ   0x03     /* Read data */
/* Requires AD[1:3] as argument (24 bit Address Data) */
/* Reads out n bytes until CS# goes high, the address is automatically increased to the next one */
#define FREAD  0x0B     /* Fast read data */
/* Requires AD[1:3] as argument (24 bit Address Data) */
/* Reads out n bytes until CS# goes high, the address is automatically increased to the next one */

#define PP     0x02     /* Page program */
/* Requires AD[1:3] as argument (24 bit Address Data) and at least one data byte (up to 256) */
/* If the eight least significan address bits are not all 0, the transmitted data that goes 
   beyond the current page will be programmed from the start address of the same page!
   
   If more than 256 data bytes are sent to the device, only the last 256 data bytes will be
   accepted and the previous data bytes will be diregarded!
   
   If 256 data bytes are going to be programmed, set LSB of Address Data to 0 */
/* Writes up to 256 bytes to the memory array */

#define SE     0x20     /* Sector erase */
/* Requires AS[1:3] as argument (24 bit Address Sector) */
/* Erases data of the chosen sector (sets to 0xFF) */
#define BE     0x52     /* or 0xD8, Block erase */
/* Requires AB[1:3] as argument (24 bit Address Block) */
/* Erases data of the chosen block (sets to 0xFF) */
#define CE     0xC7     /* Chip erase */
/* Erases data of the whole chip (sets to 0xFF) */

#define DP     0xB9     /* Deep power-down */
/* Enters deep power-down mode to minimize power consumption */
#define RDP    0xAB     /* Release from deep power-down */
/* Requires three dummy bytes as arguments */
/* Releases the chip from deep power-down mode */

#define RDID   0x9F     /* Read identification */
/* Outputs the manufacturer ID and 2-byte device ID (24 bit total) */
/* ID structure: 8-bit manufacturer ID, 8-bit memory type, 8-bit memory density */
#define RES    0xAB     /* Read electronic ID */
/* Reads out the old style of 8-bit Electronic Signature */
/* ID structure: 8-bit electronic ID in a 24-bit structure */
#define REMS   0x90     /* Read electronic manufacturer and device ID */
/* Requires two dummy bytes and ADD as third argument (ADD = 0: output manufacturer ID first, ADD = 1: output device ID first) */
/* Outputs the manufacturer ID and device ID */

/* Status register structure:

bits: 7     6     5     4     3     2     1     0
      SRWD  0     0     BP2   BP1   BP0   WEL   WIP
      
SRWD     = Status Register Write Protect,    0: SR can be written 
                                             1: SR cannot be written
                                             
BP[2:0]  = Block Protection bits,            000: none
                                             001: block 15
                                             010: block 14-15
                                             011: block 12-15
                                             100: block 8-15
                                             101 and over: all

WEL      = Write Enable Latch,               0: Memory array cannot be written
                                             1: Memory array can be written
                                             
WIP      = Write In Progress,                0: Chip is in write operation
                                             1: Chip is not in write operation
*/
#define SRWD 0x80
#define BP2  0x10
#define BP1  0x08
#define BP0  0x04
#define WEL  0x02
#define WIP  0x01

/* Memory structure:

1 block  (depends, check datasheet), usually 65536 bytes = 16 sectors
1 sector = 4096  bytes
1 page   = 256   bytes: it is virtual and not physical

Address range: 0x000000 to 0x0FFFFF
*/

struct flash_manufacturer
{
   byte id;
   char man[64];
};