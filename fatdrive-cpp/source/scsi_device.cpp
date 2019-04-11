#include <stdio.h>
#include <string.h>
#include <switch.h>

#include "scsi/scsi_context.h"

ScsiBlock::ScsiBlock(ScsiDevice *device_)
{
	device = device_;

	ScsiInquiryCommand inquiry(36);
    ScsiTestUnitReadyCommand test_unit_ready;
    ScsiReadCapacityCommand read_capacity;

    uint8_t inquiry_response[36];
    ScsiCommandStatus status = device->transfer_cmd(&inquiry, inquiry_response, 36);
    printf("Inquiry result: %x?\n", status.status);
    
    status = device->transfer_cmd(&test_unit_ready, NULL, 0);
    printf("Test unit ready result: %x\n", status.status);

    uint8_t read_capacity_response[8];
    uint32_t size_lba;
    uint32_t lba_bytes;
    status = device->transfer_cmd(&read_capacity, read_capacity_response, 8);
    printf("Read capacity result: %x\n", status.status);
    memcpy(&size_lba, &read_capacity_response[0], 4);
    size_lba = __builtin_bswap32(size_lba);
    memcpy(&lba_bytes, &read_capacity_response[4], 4);
    lba_bytes = __builtin_bswap32(lba_bytes);

    printf("Got %lu bytes (%i block size)!\n", (uint64_t)size_lba * lba_bytes, lba_bytes);

    capacity = size_lba * lba_bytes;
    block_size = lba_bytes;

    uint8_t mbr[0x200];
    read_sectors(mbr, 0, 1);

    for(int i = 0; i < 0x200; i++)
    {
        printf("%02x ", mbr[i]);
    }
    printf("\n");

    partition_infos[0] = MBRPartition::from_bytes(&mbr[0x1be]);
    partition_infos[1] = MBRPartition::from_bytes(&mbr[0x1ce]);
    partition_infos[2] = MBRPartition::from_bytes(&mbr[0x1de]);
    partition_infos[3] = MBRPartition::from_bytes(&mbr[0x1ee]);
    for(int i = 0; i < 4; i++)
    {
    	MBRPartition p = partition_infos[i];
        printf("partition %i: %02x/%02x, start %08x length %08x\n", i, p.active, p.partition_type, p.start_lba, p.num_sectors);
        if(p.partition_type != 0)
        {
        	partitions[i] = ScsiBlockPartition(this, p);
        }
    }
}

int ScsiBlock::read_sectors(uint8_t *buffer, uint32_t sector_offset, uint32_t num_sectors)
{
	ScsiRead10Command read_ten(sector_offset, block_size, num_sectors);
    ScsiCommandStatus status = device->transfer_cmd(&read_ten, buffer, num_sectors * block_size);
    printf("Read 10 response: %x\n", status.status);

    return num_sectors;
}


ScsiBlockPartition::ScsiBlockPartition(ScsiBlock *block_, MBRPartition partition_info)
{
	block = block_;
	start_lba_offset = partition_info.start_lba;
	lba_size = partition_info.num_sectors;	
}

int ScsiBlockPartition::read_sectors(uint8_t *buffer, uint32_t sector_offset, uint32_t num_sectors)
{
	// TODO: assert we don't read outside the partition
	return block->read_sectors(buffer, sector_offset + start_lba_offset, num_sectors);
}
