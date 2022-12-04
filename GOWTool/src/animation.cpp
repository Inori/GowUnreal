#include "pch.h"
#include "animation.h"

void Anime::Read(std::iostream &ss)
{
    ss.seekg(0, ios::end);
    size_t packSize = ss.tellg();
    ss.seekg(0, ios::beg);

    std::unique_ptr<uint8_t[]> packMemory = std::make_unique<uint8_t[]>(packSize);
    uint8_t* packData = packMemory.get();

    ss.read((char*)packData, packSize);

    AnimePackHeader* packHeader = (AnimePackHeader*)packData;
    AnimePackEntry* packEntries = (AnimePackEntry*)(packHeader + 1);

    for (uint32_t entryIdx = 0; entryIdx != packHeader->entryCount; ++entryIdx)
    {
        AnimePackEntry& packEntry = packEntries[entryIdx];

        AnimeGroupHeader* groupHeader = (AnimeGroupHeader*)(packData + packEntry.offset);
        AnimeGroupEntry* groupEntries = (AnimeGroupEntry*)(groupHeader + 1);
        cout << "\t" << "GroupName: " << groupHeader->name << "\n";
        cout << "\t" << "GroupType: " << groupHeader->type << "\n";

        if (groupHeader->type == 5)  // not sure this type
        {
            cout << "\t" << "Not Supported: " << groupHeader->name << "\n";
            continue;
        }

        for (uint32_t groupIdx = 0; groupIdx != groupHeader->entryCount; ++groupIdx)
        {
            AnimeGroupEntry& groupEntry = groupEntries[groupIdx];

            AnimeDefHeader* defHeader = (AnimeDefHeader*)((uint8_t*)groupHeader + groupEntry.offset);

            cout << "\t\t" << "DefName: " << defHeader->name << "\n";

            if (std::string(defHeader->name) == "navcombatidle_right#navcombatidle_right")
            {
                __debugbreak();
            }

            AnimeActionHeader* actionHeader = getAnimeAction(defHeader);
            
            cout << "\t\t\t" << "TickCount: " << actionHeader->tickCount << "\n";
            cout << "\t\t\t" << "Duration: " << actionHeader->duration << "\n";
        }
    }

}

// 1400EA2D0
AnimeOffsetBlock* Anime::getOffsetBlock(AnimeDefHeader* defHeader, uint32_t index)
{
    AnimeDefHeader* header = defHeader;
    AnimeDefEntry* entries = (AnimeDefEntry*)(header + 1);

    for (header = defHeader;
        (header->flags & 0x8000) != 0;
        entries = (AnimeDefEntry*)(header + 1),
        header = (AnimeDefHeader*)((uint8_t*)header + entries[0].size))
    {
    }

    if ((header->flags & 0x8000) == 0)
    {
        return (AnimeOffsetBlock*)((uint8_t*)header + entries[index].offset);
    }

    // step to next header and find again
    AnimeDefHeader* nextDefHeader = (AnimeDefHeader*)((uint8_t*)defHeader + entries[0].size);
    AnimeDefEntry* entry = getAnimeDefEntry(nextDefHeader, 0);
    return (AnimeOffsetBlock*)((uint8_t*)header + entry[index].offset);
}

AnimeDefEntry* Anime::getAnimeDefEntry(AnimeDefHeader* defHeader, uint32_t index)
{
    AnimeDefHeader* header = defHeader;
    AnimeDefEntry* entries = (AnimeDefEntry*)(header + 1);

    for (header = defHeader;
        (header->flags & 0x8000) != 0; 
        entries = (AnimeDefEntry*)(header + 1),
        header = (AnimeDefHeader*)((uint8_t*)header + entries[0].size))
    {
    }

    return &entries[index];
}

// 14015DB10
AnimeActionHeader* Anime::getAnimeAction(AnimeDefHeader* defHeader)
{
    AnimeActionHeader* result = nullptr;
    do 
    {
        if (!defHeader)
        {
            break;
        }

        AnimeDefEntry* entry = (AnimeDefEntry*)(defHeader + 1);
        if ((defHeader->flags & 0x8000) != 0)
        {
            AnimeDefHeader* nextHeader = (AnimeDefHeader*)((uint8_t*)defHeader + entry->size);
            entry = getAnimeDefEntry(nextHeader, 0);
        }

        if (!entry->valid)
        {
            break;
        }

        AnimeOffsetBlock* block = getOffsetBlock(defHeader, 0);
        size_t actionOffset = block->offset << 0x10 +  ((block->shift & 0xC000) << 2) | block->mask ;

        result = (AnimeActionHeader*)((uint8_t*)block + actionOffset);
    } while (false);
    return result;
}

