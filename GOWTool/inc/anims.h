#pragma once
#include "pch.h"
#include <cstdint>

#pragma pack(1)

struct AnimePackHeader
{
	uint32_t unknown0;
	uint32_t unknown1[3];
	uint64_t somePointer;  // set at runtime
	uint64_t hash;
	uint32_t unknown3;
	uint32_t unknown4;
	uint32_t unknown5;
	uint32_t packSize;
	uint16_t unknown6;
	uint16_t entryCount;
	uint32_t unknown7;
	uint32_t unknown8;
	uint32_t unknown9;
	uint64_t entrySize;  // size of entry table
};

struct AnimePackEntry
{
	uint64_t offset;
};

struct AnimeGroupHeader
{
	uint32_t unknown0[4];
	uint32_t type;
	uint32_t unknown1[2];
	uint32_t size;  // ?
	uint64_t hash0;
	char name[0x38];
	uint32_t unknown2;
	uint32_t hash1;  // ?
	uint32_t refEntryCount;
	uint32_t flags;
	uint32_t entryCount;
	uint32_t unknown5;
};

struct AnimeGroupEntry
{
    uint64_t offset;
};

struct AnimeDefHeader
{
	uint64_t hash;
	char name[0x38];
	uint32_t flags;
	uint32_t unknown1;
	uint32_t unknown2;
	uint32_t unknown3;
	float unknown4;
	float unknown5;
	float unknown6;
	float unknown7;
    uint16_t unknown8;
    uint16_t unknown14;
	uint16_t unknown9;
	uint16_t unknown10;

	uint32_t size;  // only valid when (flags & 0x8000) != 0
	uint16_t unknown12;
	uint16_t unknown13;
	uint32_t unknown15;
	uint32_t unknown16;
	uint64_t blockOffset;
};


// 000000014015DB6F
// finaleOffset = offset << 0x10 + (((shift & 0xC000) << 2) | mask)
struct AnimeOffsetBlock
{
	uint32_t unknown0;
	uint32_t offset;
	uint32_t unknown2;
	uint16_t shift;
	uint16_t mask;
};

struct AnimeDataHeader
{
	uint32_t unknown0;
	uint32_t unknown1;
	uint32_t unknown2;
    uint16_t unknown3;
    uint16_t unknown4;
	uint32_t unknown5;
	uint32_t entryOffset;
    uint32_t unknown6;
	float tickCount;
	uint16_t flags;
	uint16_t limit; // ?
	float unknown9;
	float duration;
};

struct AnimeDataEntry
{
    uint32_t unknown0;
    uint32_t unknown1;
    uint32_t offset;
    uint32_t unknown3;
};

#pragma pack()

class AnimDef
{
private:
public:
	void Read(std::iostream &ms);
};

