#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <iostream>
#include <errno.h>
#include <bcm2835.h>
#include <unistd.h>

#define WREN	0x06 // Set Write Enable Latch
#define WRDI	0x04 // Reset Write Enable Latch
#define RDSR	0x05 // Read Status Register
#define WRSR	0x01 // Write Status Register
#define READ	0x03 // Read Memory Code
#define WRITE	0x02 // Write Memory Code
#define RDID	0x9F // Read Device ID

#define PRINT_DATA(a, b, c) \
if(a) { \
printf("%s\n", a); \
} \
   for(int i = 0; i < c; i++) { \
    if(i && !(i%16)) \
    printf("\n"); \
    printf("%02X", *(b+i)); \
   } \
   printf("\n");

int write_enable(void)
{
        char buf = WREN;        
        bcm2835_spi_transfer(buf);
	//printf("[*] Write enabled!\n");
        return 0;
}

int write_disable(void)
{
        char buf = WRDI;
        bcm2835_spi_transfer(buf);
        //printf("\n[*] Write disabled!\n");
        return 0;
}

int read_byte(uint16_t addr)
{
	char buf[4];
	if (addr > 0x2000)
	{
		printf("[-] Address out of bounds\n");
		return 1;
	}
	printf("Reading from 0x%04x\n", addr);

	buf[0] = READ;
	buf[1] = addr >> 8;
	buf[2] = addr;
	buf[3] = 0x00;
	bcm2835_spi_transfern(buf, 4);

	printf("Data: %02x\n", buf[3]);
	return 0;
}
	
int write_byte(uint16_t addr, uint8_t data)
{
	char buf[4] = {0};
	
	if (addr > 0x2000)
	{
		printf("[-] Address out of bounds!\n");
		return 1;
	}
	
	write_enable();
	printf("Writing %02x to %04x\n", data, addr);
	
	buf[0] = WRITE;
	buf[1] = addr >> 8;
	buf[2] = addr;
	buf[3] = data;
	bcm2835_spi_transfern(buf, 4);
	
	write_disable();
	memset(buf, 0, sizeof(buf));
	
	buf[0] = READ;
	buf[1] = addr >> 8;
	buf[2] = addr;
	buf[3] = 0x00;
	bcm2835_spi_transfern(buf, 4);
	
	if (buf[3] != data)
	{
		printf("[-] Bad write, data not match! %x %x\n", buf[3], data);
		return 1;
	}
	printf("[+] WRITE OKAY!\n");
	return 0;
}

int dump_bytes(uint16_t addr, uint16_t len)
{
	char buf[4] = {0};
	char data[len] = {0};
	uint16_t count = 0;	

	if (addr > 0x2000)
	{
		printf("[-] Address out of bounds!\n");
		return 1;
	}
	if ((addr+len) > 0x2000)
	{
		printf("[-] Data out of bounds!\n");
		return 1;
	}
	
	printf("Dumping %d bytes from 0x%04x\n", len, addr);
	
	do {
		buf[0] = READ;
		buf[1] = addr >> 8;
		buf[2] = addr;
		buf[3] = 0x00;
		bcm2835_spi_transfern(buf, 4);
		
		data[count] = buf[3];
		addr++;
		count += 1;
		printf("\rBytes read: %d", count);
		fflush(stdout);
	} while (count < len);
	
	PRINT_DATA("\n[+] Dumping...\n", data, len);
	return 0;
}

int erase_all(void)
{
	char buf[4];
	uint16_t addr = 0;
	
	printf("Erasing chip...\n");

	do {
		write_enable();
		buf[0] = WRITE;
		buf[1] = addr >> 8;
		buf[2] = addr;
		buf[3] = 0x00;
		bcm2835_spi_transfern(buf, 4);
		
		addr++;
		write_disable();
		printf("\rErasing bytes: %d", addr);
	} while (addr < 0x2000);
	printf("\n[+] ERASE OKAY!\n");
	return 0;
}

int initialize_fram(void)
{
	char buf[5] = {0};	

	if (!bcm2835_init())
	{
		printf("[-] bcm2835_init failed. Are you running as root?\n");
		return 1;
	}
	
	if (!bcm2835_spi_begin())
	{
		printf("[-] bcm2835_spi_begin failed. Are you running as root?\n");
		return 1;
	}
	
	bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
	bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
	bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_32);
	bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
	bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
	
	buf[0] = RDID;
	bcm2835_spi_transfern(buf, 5);
	
	printf("Manufacturer ID: %02x%02x Product ID: %02x%02x\n", buf[1], buf[2], buf[3], buf[4]);
	
	return 0;
}

int main(int argc, char **argv)
{
	char data;
	uint16_t addr;
	uint16_t len;
	
	if (argc < 2)
	{
		printf("\nSPI FRAM Utility\n");
		printf("----------------\n");
		printf("read [16-bit addr]\n");
		printf("write [16-bit addr] [data]\n");
		printf("\n");
		return 0;
	}
	
	initialize_fram();
	
	if (strcmp("read", argv[1]) == 0)
	{
		if (argv[2] == NULL)
		{
			addr = 0x0000;
			read_byte(addr);
		}
		sscanf(argv[2], "%x", &addr);
		read_byte(addr);
		printf("[+] Okay!\n");
		return 0;
	}
	
	if (strcmp("write", argv[1]) == 0)
	{
		if (argv[2] == NULL)
		{
			printf("[-] Need address for write!\n");
			return 1;
		}
		if (argv[3] == NULL)
		{
			printf("[-] Need data for write!\n");
			return 1;
		}
		sscanf(argv[2], "%x", &addr);
		sscanf(argv[3], "%x", &data);
	
		write_byte(addr, data);
	}
	
	if (strcmp("dump", argv[1]) == 0)
	{
		if (argv[2] == NULL)
		{
			printf("[-] Need address for dump!\n");
			return 1;
		}
		if (argv[3] == NULL)
		{
			printf("[-] Need length for dump!\n");
			return 1;
		}
		sscanf(argv[2], "%x", &addr);
		sscanf(argv[3], "%x", &len);
	
		dump_bytes(addr, len);
	}
	if (strcmp("erase_all", argv[1]) == 0)
	{
		erase_all();
	}

	return 0;
}
