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

// 000000014056A500
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

// 00000001405C72F0
struct AnimeDefHeader
{
	uint64_t hash;
	char name[0x38];
	uint32_t flags;
	uint32_t someOffset;  //  00000001405C5ED8: only valid when (flags & 0x40000000) != 0
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
};

// 00000001400EA2D0
struct AnimeDefEntry
{
    uint32_t size;
    uint16_t valid;
    uint16_t unknown0;
	uint64_t unknown1;
	uint64_t offset;
}; 


// 000000014015DB6F
// finaleOffset = offset << 0x10 + (( (shift & 0xC000) << 2) | mask)
struct AnimeOffsetBlock
{
	uint32_t unknown0;
	uint32_t offset;
	uint32_t unknown2;
	uint16_t shift;
	uint16_t mask;
};


// 0000000140B17E90
struct AnimeActionHeader
{
	uint32_t unknown0;
	uint32_t unknown1;
	uint32_t unknown2;
    uint16_t entryCount;
    uint16_t unknown4;

	uint32_t unknown5;
	uint32_t entryOffset;
    uint32_t unknown6;
	float tickCount;

	uint16_t flags;
	uint16_t limit; // ? 0000000140B2F871
	float unknown9;
	float duration;
	uint32_t unknown7;
};

// 0000000140B32030
struct AnimeActionEntry
{
    uint32_t dispatchTableOffset;  // 0000000140B320A0
    uint32_t unknown1;
    uint32_t offset;
    uint16_t valid;
	uint16_t unknown3;
};

struct AnimeDispatchTable
{
	uint8_t dispatchId; // function table (0x14107C0B0) index
	uint8_t wordCountMinusOne;  // table size = (wordCountMinusOne + 1) * 2
};

#pragma pack()

class Anime
{
public:
	void Read(std::iostream &ss);

private:
	AnimeOffsetBlock* getOffsetBlock(AnimeDefHeader* defHeader, uint32_t index);
	AnimeDefEntry* getAnimeDefEntry(AnimeDefHeader* defHeader, uint32_t index);
	AnimeActionHeader* getAnimeAction(AnimeDefHeader* defHeader);
};

