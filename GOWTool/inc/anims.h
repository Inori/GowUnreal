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
	uint32_t unknown11;
	uint16_t unknown12;
	uint16_t unknown13;
};

#pragma pack()

class AnimDef
{
private:
public:
	void Read(std::iostream &ms);
};

