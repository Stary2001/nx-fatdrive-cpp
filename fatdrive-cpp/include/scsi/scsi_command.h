enum class ScsiDirection
{
	None,
	In,
	Out
};

#define CBW_SIGNATURE 0x43425355

#define COMMAND_PASSED 0
#define COMMAND_FAILED 1
#define PHASE_ERROR 2
#define CSW_SIZE 13
#define CSW_SIGNATURE 0x53425355

class ScsiBuffer
{
public:
	int idx = 0;
	uint8_t storage[31];

	ScsiBuffer()
	{
		memset(storage, 0, sizeof(storage));
	}

	void write_u8(uint8_t e)
	{
		memcpy(&storage[idx], &e, sizeof(e));
		idx++;
	}

	void padding(int n)
	{
		idx += n;
	}

	void write_u16_be(uint16_t e)
	{
		e = __builtin_bswap16(e);
		memcpy(&storage[idx], &e, sizeof(e));
		idx += 2;
	}

	void write_u32(uint32_t e)
	{
		memcpy(&storage[idx], &e, sizeof(e));
		idx += 4;
	}

	void write_u32_be(uint32_t e)
	{
		e = __builtin_bswap32(e);
		memcpy(&storage[idx], &e, sizeof(e));
		idx += 4;
	}
};

class ScsiCommand
{ 
public:
	uint32_t tag;
    uint32_t data_transfer_length;
    uint8_t flags;
    uint8_t lun;
    uint8_t cb_length;
    ScsiDirection direction;

    ScsiCommand(uint32_t data_transfer_length_, ScsiDirection direction_, uint8_t lun_, uint8_t cb_length_)
    {
    	tag = 0; // should this be default?

    	data_transfer_length = data_transfer_length_;
    	direction = direction_;
    	lun = lun_;
    	cb_length = cb_length_;

    	if(direction == ScsiDirection::In)
    	{
    		flags = 0x80;
    	}
    	else
    	{
    		flags = 0;
    	}
    }

    virtual void to_bytes(uint8_t *out) = 0;
    void write_header(ScsiBuffer &out)
    {
    	out.write_u32(CBW_SIGNATURE);
    	out.write_u32(tag);
    	out.write_u32(data_transfer_length);
    	out.write_u8(flags);
    	out.write_u8(lun);
    	out.write_u8(cb_length);
    };
};

class ScsiInquiryCommand : public ScsiCommand
{
public:
	uint8_t allocation_length;
	uint8_t opcode;

	ScsiInquiryCommand(uint8_t allocation_length_) :
	ScsiCommand(allocation_length_, ScsiDirection::In, 0, /* length */ 5)
	{
		opcode = 0x12;
		allocation_length = allocation_length_;
	}

	virtual void to_bytes(uint8_t *out)
	{
		ScsiBuffer b;
		write_header(b);
		b.write_u8(opcode);
		b.padding(3);
		b.write_u8(allocation_length);

		memcpy(out, b.storage, 31);
	}
};


class ScsiTestUnitReadyCommand : public ScsiCommand
{
public:
	uint8_t opcode;
	
	ScsiTestUnitReadyCommand() :
	ScsiCommand(0, ScsiDirection::None, 0, /* length */ 0x6)
	{
		opcode = 0;
	}

	virtual void to_bytes(uint8_t *out)
	{
		ScsiBuffer b;
		write_header(b);
		b.write_u8(opcode);

		memcpy(out, b.storage, 31);
	}
};


class ScsiReadCapacityCommand : public ScsiCommand
{
public:
	uint8_t opcode;
	
	ScsiReadCapacityCommand() :
	ScsiCommand(0x8, ScsiDirection::In, 0, /* length */ 0x10)
	{
		opcode = 0x25;
	}

	virtual void to_bytes(uint8_t *out)
	{
		ScsiBuffer b;
		write_header(b);
		b.write_u8(opcode);

		memcpy(out, b.storage, 31);
	}
};


class ScsiRead10Command : public ScsiCommand
{
public:
	uint8_t opcode;
	uint32_t block_address;
	uint16_t transfer_blocks;
	
	ScsiRead10Command(uint32_t block_address_, uint32_t block_size, uint16_t transfer_blocks_) :
	ScsiCommand(transfer_blocks_ * block_size, ScsiDirection::In, 0, /* length */ 10)
	{
		opcode = 0x28;
		block_address = block_address_;
		transfer_blocks = transfer_blocks_;
	}

	virtual void to_bytes(uint8_t *out)
	{
		ScsiBuffer b;
		write_header(b);
		b.write_u8(opcode);
		b.padding(1);
		b.write_u32_be(block_address);
		b.padding(1);
		b.write_u16_be(transfer_blocks);
		b.padding(1);

		memcpy(out, b.storage, 31);
	}
};


class ScsiCommandStatus
{
public:
	uint32_t signature;
	uint32_t tag;
	uint32_t data_residue;
	uint8_t status;

	ScsiCommandStatus()
	{}

	ScsiCommandStatus(uint8_t csw[13])
	{
		memcpy(&signature, &csw[0], 4);
		memcpy(&tag, &csw[4], 4);
		memcpy(&data_residue, &csw[8], 4);
		memcpy(&status, &csw[12], 1);
	}
};