CC = gcc
INCLUDES = -I ./

all:
	$(CC) $(INCLUDES) spi_fram.c -lbcm2835 -o spi_fram

clean:
	rm spi_fram

