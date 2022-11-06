#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

typedef struct
{
	uint8_t status;
	uint8_t CHS_start[3];
	uint8_t type;
	uint8_t CHS_end[3];
	uint32_t starting_sector;
	uint32_t sectors_num;
} MBR;
typedef struct
{
	uint64_t Signature;
	uint32_t Revision;
	uint32_t Header_size;
	uint32_t CRC32_of_header;
	uint32_t Reserved;
	uint64_t Current_LBA;
	uint64_t Backup_LBA;
	uint64_t First_usable_LBA;
	uint64_t Last_usable_LBA;
	uint64_t Disk_GUID[2];
	uint32_t Starting_of_partitions_entries;
	uint32_t Number_of_partition_entries;
	uint32_t size_of_partition_entrie;
	uint32_t CRC32_of_entries;
	uint8_t reserved[420];
} GPT_header;

typedef struct
{
	uint64_t Partition_type_GUID[2];
	uint64_t Unique_Partition_GUID[2];
	uint64_t First_LBA;
	uint64_t Last_LBA;
	uint64_t Attribute_flags;
	uint8_t Partition_name[72];
} GPT_entry;

uint32_t start_of_extended = 0;
void printGPT(GPT_entry *partition_table, uint8_t partition_num, char *device_name);
void parseGPT(char *device_name)
{
	GPT_header GPTHeader;
	GPT_entry GPTEntry;
	int device = open(device_name, O_RDONLY);
	read(device, &GPTHeader, 512); // read first sector which is protective MBR
	read(device, &GPTHeader, 512);
	lseek(device, (off_t)GPTHeader.Starting_of_partitions_entries * 512, SEEK_SET);
	printf("%-11s", "Device");
	if (strlen(device_name) > 11)
	{
		for (int i = 0; i < strlen(device_name) - 8; i++)
		{
			printf(" ");
		}
	}
	printf("%11s%11s%11s%11s\n","Start", "End",
		   "Sectors", "Size");

	for (uint8_t i = 0; i < 10; i++)
	{
		read(device, &GPTEntry, 128);

		if(GPTEntry.First_LBA != 0)
		{
			printGPT(&GPTEntry,i+1,device_name);
		}
	}
}
void printMBR(MBR *partition_table, uint8_t partition_num, char *device_name);
void parsePrimary(char *device_name)
{
	uint8_t buf[512];
	int device = open(device_name, O_RDONLY);
	if (device == -1)
	{
		fprintf(stderr, "myfdisk: %s: %s\n", device_name, strerror(errno));
		exit(errno);
	}
	else
	{

		read(device, buf, 512);
		uint16_t *signature = (uint16_t *)&buf[510];
		if (*signature == 0xaa55)
		{
			MBR *partition_table = (MBR *)&buf[446];

			if (partition_table->type == 0xee)
			{
				parseGPT(device_name);
			}
			else
			{
				printf("%-11s", "Device");
				if (strlen(device_name) > 11)
				{
					for (int i = 0; i < strlen(device_name) - 8; i++)
					{
						printf(" ");
					}
				}
				printf("%5s%11s%11s%11s%11s%3s%7s\n", "Boot", "Start", "End",
					   "Sectors", "Size", "Id", "Type");

				for (uint8_t i = 0; i < 4; i++)
				{
					if (partition_table[i].type == 5)
					{
						start_of_extended = partition_table[i].starting_sector;
					}

					if (partition_table[i].type != 0)
						printMBR(&partition_table[i], i + 1, device_name);
				}
			}
		}
		else
		{
			printf("this device isn't an MBR\n");
		}
	}
}

void parseExtended(char *device_name)
{
	MBR *partition_table;
	uint8_t buf[512];
	int device = open(device_name, O_RDONLY);
	off_t offest = 0;
	uint8_t index = 5;

	if (start_of_extended)
	{
		offest = start_of_extended;
		do
		{
			lseek(device, offest * 512, SEEK_SET);
			read(device, buf, 512);
			partition_table = (MBR *)&buf[446];
			partition_table->starting_sector += offest;
			printMBR(partition_table, index++, device_name);
			partition_table++;
			offest = partition_table->starting_sector + start_of_extended;
			// printf("%u\n",partition_table->starting_sector+starts_of_extended[i]);
		} while (partition_table->type);
	}
}

void printMBR(MBR *partition_table, uint8_t partition_num, char *device_name)
{

	float size = 0;

	printf("%-8s%-3d", device_name, partition_num);
	printf("%5s", partition_table->status == 0x80 ? "*" : " ");
	printf("%11u", partition_table->starting_sector);
	printf("%11u", (partition_table->starting_sector +
					(partition_table->sectors_num) - 1));
	printf("%11u", partition_table->sectors_num);
	size = ((float)partition_table->sectors_num / (1024 * 1024 * 1024)) * 512;
	if ((int)size >= 1)
		printf("%10.1f%-1s", size, "G");
	else
	{
		size *= 1024;
		printf("%10d%-1s", (int)size, "M");
	}
	printf("%3x\n", partition_table->type);
}
void printGPT(GPT_entry *partition_table, uint8_t partition_num, char *device_name)
{

	float size = 0;
	uint32_t sectors_num =partition_table->Last_LBA - partition_table->First_LBA +1;
	printf("%-8s%-3d", device_name, partition_num);
	printf("%11lu", partition_table->First_LBA);
	printf("%11lu", partition_table->Last_LBA);
	printf("%11u", sectors_num);
	size = ((float)sectors_num / (1024 * 1024 * 1024)) * 512;
	if ((int)size >= 1)
		printf("%10.1f%-4s\n", size, "G");
	else
	{
		size *= 1024;
		printf("%10d%-4s\n", (int)size, "M");
	}
	//printf("%10lx\n", partition_table->Partition_type_GUID[1]);
}

int main(int argc, char **argv)
{
	if (argc == 2)
	{
		parsePrimary(argv[1]);
		parseExtended(argv[1]);
	}
	else
	{
		printf("myfdisk: device name is missing\n");
	}
	return 0;
}
