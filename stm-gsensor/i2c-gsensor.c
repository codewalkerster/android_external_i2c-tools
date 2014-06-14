/*
 *   G-Sensor Reader
 *   Reads data from i2c interface.
 *   Supported chips:
 *     - ST Micro electronics LIS302DL (with 0x3B magic)
 *
 *   Android KitKat-x86 Port by Tanguy Pruvot <tpruvot@github>
 *   Copyright (C) 2008       Esteve Espuna <esteve@eslack.org>
 *                            Pau Oliva <pof@eslack.org>
 *
 *   Based on i2cget.c:
 *   Copyright (C) 2005       Jean Delvare <khali@linux-fr.org>
 *
 *   Based on i2cset.c, i2cbusses.c:
 *   Copyright (C) 2001-2003  Frodo Looijaard <frodol@dds.nl>, and
 *                            Mark D. Studebaker <mdsxyz123@yahoo.com>
 *   Copyright (C) 2004-2005  Jean Delvare <khali@linux-fr.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *   MA 02110-1301 USA.
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

#include <i2c/smbus.h>

enum lis3_reg {
	WHO_AM_I	= 0x0F,
	OFFSET_X	= 0x16,
	OFFSET_Y	= 0x17,
	OFFSET_Z	= 0x18,
	GAIN_X		= 0x19,
	GAIN_Y		= 0x1A,
	GAIN_Z		= 0x1B,
	CTRL_REG1	= 0x20,
	CTRL_REG2	= 0x21,
	CTRL_REG3	= 0x22,
	FILTER_RST	= 0x23,
	STATUS_REG	= 0x27,
	OUTX_L		= 0x28,
	OUTX_H		= 0x29,
	OUTX		= 0x29,
	OUTY_L		= 0x2A,
	OUTY_H		= 0x2B,
	OUTY		= 0x2B,
	OUTZ_L		= 0x2C,
	OUTZ_H		= 0x2D,
	OUTZ		= 0x2D,
};

enum lis3_who_am_i {
	WAI_3DLH	= 0x32,	/* 16 bits: LIS331DLH */
	WAI_3DC		= 0x33,	/* 8 bits: LIS3DC, HP3DC */
	WAI_12B		= 0x3A, /* 12 bits: LIS3LV02D[LQ]... */
	WAI_8B		= 0x3B, /* 8 bits: LIS[23]02D[LQ]... */
	WAI_6B		= 0x52, /* 6 bits: LIS331DLF - not supported */
};

/* if detected by kernel, you can read position here */
#define driver_position "/sys/devices/platform/lis3lv02d/position"

int open_i2c_dev(const int i2cbus, const int quiet)
{
	int file;
	char filename[20];

	sprintf(filename, "/dev/i2c/%d", i2cbus);
	file = open(filename, O_RDWR);

	if (file < 0 && errno == ENOENT) {
		sprintf(filename, "/dev/i2c-%d", i2cbus);
		file = open(filename, O_RDWR);
	}

	if (file < 0 && !quiet) {
		if (errno == ENOENT) {
			fprintf(stderr, "Error: Could not open file "
			        "`/dev/i2c-%d' or `/dev/i2c/%d': %s\n",
			        i2cbus, i2cbus, strerror(ENOENT));
		} else {
			fprintf(stderr, "Error: Could not open file "
			        "`%s': %s\n", filename, strerror(errno));
			if (errno == EACCES)
				fprintf(stderr, "Run as root?\n");
		}
	}

	return file;
}

int main(int argc, char *argv[])
{
	int i2cbus, address, file, loop;
	short xh, yh, zh;
	short x,y,z;
	short magic;

	loop = 0;
	if (argc >= 2) {
		i2cbus = atoi(argv[1]);
		if (argc == 3 && strcmp(argv[2], "loop") == 0)
			loop = 1;
	} else {
		fprintf(stderr, "Usage : %s i2cbus [loop]\n", argv[0]);
		exit(1);
	}

	file = open_i2c_dev(i2cbus, 0);

	address = 0x1D;
	ioctl(file, I2C_SLAVE, address);

	i2c_smbus_write_byte_data(file, CTRL_REG1, 0x47);
	i2c_smbus_write_byte_data(file, CTRL_REG2, 0x63);

	magic = i2c_smbus_read_byte_data(file, 0x0F);
	if (magic != WAI_8B && magic != WAI_12B) {
		printf("Accelerometer not found %d\n", i2cbus);
		exit(-1);
	}

	while (1)
	{
		if (magic == WAI_12B) {
			xh = i2c_smbus_read_byte_data(file, OUTX_H);
			x = (xh<<8) | i2c_smbus_read_byte_data(file, OUTX_L);
			yh = i2c_smbus_read_byte_data(file, OUTY_H);
			y = (yh<<8) | i2c_smbus_read_byte_data(file, OUTY_L);
			zh = i2c_smbus_read_byte_data(file, OUTZ_H);
			z = (zh<<8) | i2c_smbus_read_byte_data(file, OUTZ_L);
		} else {
			x = i2c_smbus_read_byte_data(file, OUTX);
			y = i2c_smbus_read_byte_data(file, OUTY);
			z = i2c_smbus_read_byte_data(file, OUTZ);
		}

		if (x < 128)
			printf("normal\n");
		else if (y < 128)
			printf("left\n");
		else if (z < 128)
			printf("right\n");
		else
			printf("inverted\n");

		if (!loop)
			break;

		sleep(2);
	}
	close(file);

	exit(0);
}
