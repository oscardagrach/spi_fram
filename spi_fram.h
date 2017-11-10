#define WREN	0x06 // Set Write Enable Latch
#define WRDI	0x04 // Reset Write Enable Latch
#define RDSR	0x05 // Read Status Register
#define WRSR	0x01 // Write Status Register
#define READ	0x03 // Read Memory Code
#define WRITE	0x02 // Write Memory Code
#define RDID	0x9F // Read Device ID

static int spi_init = 0;

enum boundary {
	ADDR_CHECK,
	DATA_CHECK,
	BOTH_CHECK
};
