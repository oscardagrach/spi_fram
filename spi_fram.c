#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <bcm2835.h>

#include "spi_fram.h"

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

void write_enable(void)
{       
        bcm2835_spi_transfer((uint8_t)WREN);
}

void write_disable(void)
{
        bcm2835_spi_transfer((uint8_t)WRDI);
}

int spi_ready(void)
{
	if (spi_init == 1)
	{
		return 0;
	}
	
	return -1;
}

int check_boundary(uint8_t type, uint16_t addr, uint16_t len)
{
	switch(type)
	{
		case ADDR_CHECK:
			if (addr > 0x2000)
			{
				printf("[-] Address out of bounds\n");
				return 1;
			}
			break;
		case DATA_CHECK:
			if (addr+len > 0x2000)
			{
				printf("[-] Data out of bounds\n");
				return 1;
			}
			break;
		case BOTH_CHECK:
			if (addr > 0x2000)
			{
				printf("[-] Address out of bounds\n");
				return 1;
			}
			if (addr+len > 0x2000)
			{
				printf("[-] Data out of bounds\n");
				return 1;
			}
			break;
		default:
			return -1;
	}
	return 0;
}


int read_byte(uint16_t addr)
{
	int err;
	char buf[4];

	err = spi_ready();
	if (err)
	{
		printf("SPI/FRAM is not initialized\n");
		return -1;
	}
	
	err = check_boundary(ADDR_CHECK, addr, 0);
	if (err) return 1;

	printf("Reading from 0x%04hX\n", addr);

	buf[0] = READ;
	buf[1] = addr >> 8;
	buf[2] = addr;
	buf[3] = 0x00;
	bcm2835_spi_transfern(buf, 4);

	printf("Data: %02hhX\n\n", buf[3]);
	printf("[+] READ OKAY!\n");
	return 0;
}
	
int write_byte(uint16_t addr, uint8_t data)
{
	int err;
	char buf[4] = {0};

	err = spi_ready();
	if (err)
	{
		printf("SPI/FRAM is not initialized\n");
		return -1;
	}

	err = check_boundary(ADDR_CHECK, addr, 0);
	if (err) return 1;
	
	write_enable();
	printf("Writing 0x%02hhX to 0x%04hX\n", data, addr);
	
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
		printf("[-] Bad write, data not match! %hhX %hhXx\n", buf[3], data);
		return 1;
	}
	printf("\n[+] WRITE OKAY!\n");
	return 0;
}

int dump_bytes(uint16_t addr, uint16_t len)
{
	int err;
	char buf[4] = {0};
	uint8_t * data;
	uint16_t count = 0;

	err = spi_ready();
	if (err)
	{
		printf("SPI/FRAM is not initialized\n");
		return -1;
	}

	err = check_boundary(BOTH_CHECK, addr, len);
	if (err) return 1;
	
	data = (uint8_t*) malloc(sizeof(uint8_t)*len);
	if (!data)
	{
		printf("Error allocating buffer!\n");
		return 1;
	}

	printf("Dumping %d bytes from 0x%hX\n", len, addr);
	
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
	
	PRINT_DATA("\nDumping...\n", data, len);
	printf("[+] DUMP OKAY!\n");
	return 0;
}

int erase_all(void)
{
	int err = 0;
	char buf[4];
	uint16_t addr = 0;

	err = spi_ready();
	if (err)
	{
		printf("SPI/FRAM is not initialized\n");
		return -1;
	}
	
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

int write_image(uint16_t addr, const char *file)
{
	FILE *inf;
	int err = 0;
	char buf[4] = {0};
	uint16_t size;
	uint16_t count = 0;
	uint8_t * data;
	uint16_t result;

	err = spi_ready();
	if (err)
	{
		printf("SPI/FRAM is not initialized\n");
		return -1;
	}
	
	inf = fopen(file, "rb");
	if (!inf)
	{
		printf("[-] Error opening file!\n");
		return 1;
	}
	
	fseek(inf, 0, SEEK_END);
	size = ftell(inf);
	rewind(inf);
	
	err = check_boundary(BOTH_CHECK, addr, size);
	if (err) return 1;
	
	data = (uint8_t*) malloc(sizeof(uint8_t)*size);
	if (!data)
	{
		printf("[-] Failed to allocate buffer\n");
		return 1;
	}
	
	result = fread(data, 1, size, inf);
	
	if (result != size)
	{
		printf("[-] Read file error!\n");
		return 1;
	}
	
	fclose(inf);
	printf("Writing %d bytes from %s...\n", size, file);
	
	do {
		write_enable();
		buf[0] = WRITE;
		buf[1] = addr >> 8;
		buf[2] = addr;
		buf[3] = data[count];
		bcm2835_spi_transfern(buf, 4);
		write_disable();

		count++;
		addr++;
		printf("\rWriting bytes: %d", count);
		fflush(stdout);
	} while (count < size);
	printf("\n[+] WRITE IMAGE OKAY!\n");
	return 0;
}

int read_image(uint16_t addr, uint16_t len, const char *file)
{
	FILE *outf;
	int err;
	char buf[4] = {0};
	uint8_t * data;
	uint16_t count = 0;

	err = spi_ready();
	if (err)
	{
		printf("SPI/FRAM is not initialized\n");
		return -1;
	}
	
	err = check_boundary(BOTH_CHECK, addr, len);
	if (err) return 1;

	data = (uint8_t*) malloc(sizeof(uint8_t)*len);
	if (!data)
	{
		printf("[-] Error allocating memory!\n");
		return 1;
	}
	
	printf("Reading %d bytes to %s...\n", len, file);
	
	do {
		buf[0] = READ;
		buf[1] = addr >> 8;
		buf[2] = addr;
		buf[3] = 0x00;
		bcm2835_spi_transfern(buf, 4);
		data[count] = buf[3];
		addr++;
		count++;
		printf("\rBytes read: %d", count);
		fflush(stdout);
	} while (count < len);

	outf = fopen(file, "wb");
	if (!outf)
	{
		printf("\n[-] Failed to open file %s\n", file);
		return 1;
	}
	if (!fwrite(data, 1, len, outf))
	{
		printf("\n[-] Failed to write file %s\n", file);
		fclose(outf);
		return 1;
	}
	fsync(fileno(outf));
	fclose(outf);
	printf("\n[+] READ IMAGE OKAY!\n");
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
	
	printf("\nDetecting chip...\n");
	buf[0] = RDID;
	bcm2835_spi_transfern(buf, 5);
	printf("\n-------------------------------------------\n");
	printf("[*] Manufacturer ID: %02x%02x Product ID: %02x%02x\n", buf[1], buf[2], buf[3], buf[4]);
	printf("-------------------------------------------\n\n");
	
	if (buf[1] == 0x04)
	{
		spi_init = 1;
	}

	return 0;
}

int main(int argc, char **argv)
{
	char file[200] = {0};
	char data;
	uint16_t addr;
	uint16_t len;
	
	if (argc < 2)
	{
		printf("\nSPI FRAM Utility\n");
		printf("----------------\n");
		printf("read: [16-bit addr]\n");
		printf("write: [16-bit addr] [data]\n");
		printf("erase_all\n");
		printf("read_image: [16-bit addr] [len] [file]\n");
		printf("write_image: [16-bit addr] [file]\n");
		printf("\n");
		return 0;
	}
	
	initialize_fram();
	
	if (strcmp("read", argv[1]) == 0)
	{
		if (argc != 3)
		{
			printf("Wrong args!\n");
			return 1;
		}
		sscanf(argv[2], "%hx", &addr);
		read_byte(addr);
		return 0;
	}
	if (strcmp("write", argv[1]) == 0)
	{
		if (argc != 4)
		{
			printf("Wrong args!\n");
			return 1;
		}
		sscanf(argv[2], "%hx", &addr);
		sscanf(argv[3], "%hhx", &data);
	
		write_byte(addr, data);
		return 0;
	}
	if (strcmp("dump", argv[1]) == 0)
	{
		if (argc != 4)
		{
			printf("Wrong args!\n");
		}
		sscanf(argv[2], "%hx", &addr);
		sscanf(argv[3], "%hx", &len);
	
		dump_bytes(addr, len);
		return 0;
	}
	if (strcmp("erase_all", argv[1]) == 0)
	{
		erase_all();
		return 0;
	}
	if (strcmp("read_image", argv[1]) == 0)
	{
		if (argc != 5)
		{
			printf("Wrong args\n");
			return 1;
		}
		sscanf(argv[2], "%hx", &addr);
		sscanf(argv[3], "%hx", &len);
		sscanf(argv[4], "%s", file);

		read_image(addr, len, file);
		return 0;
	}
	if (strcmp("write_image", argv[1]) == 0)
	{
		if (argc != 4)
		{
			printf("Wrong args!\n");
		}

		sscanf(argv[2], "%hx", &addr);
		sscanf(argv[3], "%s", file);
		write_image(addr, file);
		return 0;
	}
	printf("[-] Unrecognized command!\n");
	return 0;
}

