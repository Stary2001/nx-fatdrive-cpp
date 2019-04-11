#include "scsi_command.h"
#include <malloc.h>

class ScsiDevice
{
public:

	uint8_t *usb_bounce_buffer_a;
	uint8_t *usb_bounce_buffer_b;
	uint8_t *usb_bounce_buffer_c;

	UsbHsClientIfSession *client;
	UsbHsClientEpSession *in_endpoint;
	UsbHsClientEpSession *out_endpoint;

	ScsiDevice(UsbHsClientIfSession *client_, UsbHsClientEpSession *in_endpoint_, UsbHsClientEpSession *out_endpoint_)
	{
		usb_bounce_buffer_a = (uint8_t*) memalign(0x1000, 0x1000);
		usb_bounce_buffer_b = (uint8_t*) memalign(0x1000, 0x1000);
		usb_bounce_buffer_c = (uint8_t*) memalign(0x1000, 0x1000);
		client = client_;
		in_endpoint = in_endpoint_;
		out_endpoint = out_endpoint_;
	}

	ScsiCommandStatus read_csw();
	void push_cmd(ScsiCommand *c);
	ScsiCommandStatus transfer_cmd(ScsiCommand *c, uint8_t *buffer, size_t buffer_size);
};


class MBRPartition
{
public:
    uint8_t active;
    uint8_t partition_type;
    uint32_t start_lba;
    uint32_t num_sectors;

    static MBRPartition from_bytes(uint8_t *entry)
    {
        MBRPartition p;
        p.active = entry[0];
        // 1 - 3 are chs start, skip them
        p.partition_type = entry[4];
        // 5 - 7 are chs end, skip them
        memcpy(&p.start_lba, &entry[8], 4);
        memcpy(&p.num_sectors, &entry[12], 4);
        return p;
    }
};

class ScsiBlock;
class ScsiBlockPartition
{
public:
	ScsiBlock *block;
	uint32_t start_lba_offset;
	uint32_t lba_size;
	ScsiBlockPartition() {}
	ScsiBlockPartition(ScsiBlock *block_, MBRPartition partition_info);
	int read_sectors(uint8_t *buffer, uint32_t sector_offset, uint32_t num_sectors);
};

class ScsiBlock
{
public:
	uint64_t capacity;
	uint32_t block_size;

	ScsiBlockPartition partitions[4];
	MBRPartition partition_infos[4];

	ScsiDevice *device;
	ScsiBlock(ScsiDevice *device_);
	int read_sectors(uint8_t *buffer, uint32_t sector_offset, uint32_t num_sectors);
};