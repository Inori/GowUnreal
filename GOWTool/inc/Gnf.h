#pragma once
#include "pch.h"

namespace Gnf
{
    enum class Format : uint32_t
    {
        FormatInvalid = 0x0,
        Format8 = 0x1,
        Format16 = 0x2,
        Format8_8 = 0x3,
        Format32 = 0x4,
        Format16_16 = 0x5,
        Format10_11_11 = 0x6,
        Format11_11_10 = 0x7,
        Format10_10_10_2 = 0x8,
        Format2_10_10_10 = 0x9,
        Format8_8_8_8 = 0xa,
        Format32_32 = 0xb,
        Format16_16_16_16 = 0xc,
        Format32_32_32 = 0xd,
        Format32_32_32_32 = 0xe,
        FormatReserved_15 = 0xf,
        Format5_6_5 = 0x10,
        Format1_5_5_5 = 0x11,
        Format5_5_5_1 = 0x12,
        Format4_4_4_4 = 0x13,
        Format8_24 = 0x14,
        Format24_8 = 0x15,
        FormatX24_8_32 = 0x16,
        FormatReserved_23 = 0x17,
        FormatReserved_24 = 0x18,
        FormatReserved_25 = 0x19,
        FormatReserved_26 = 0x1a,
        FormatReserved_27 = 0x1b,
        FormatReserved_28 = 0x1c,
        FormatReserved_29 = 0x1d,
        FormatReserved_30 = 0x1e,
        FormatReserved_31 = 0x1f,
        FormatGB_GR = 0x20,
        FormatBG_RG = 0x21,
        Format5_9_9_9 = 0x22,
        FormatBC1 = 0x23,
        FormatBC2 = 0x24,
        FormatBC3 = 0x25,
        FormatBC4 = 0x26,
        FormatBC5 = 0x27,
        FormatBC6 = 0x28,
        FormatBC7 = 0x29,
        FormatReserved_42 = 0x2a,
        FormatReserved_43 = 0x2b,
        FormatFMask8_S2_F1 = 0x2c,
        FormatFMask8_S4_F1 = 0x2d,
        FormatFMask8_S8_F1 = 0x2e,
        FormatFMask8_S2_F2 = 0x2f,
        FormatFMask8_S4_F2 = 0x30,
        FormatFMask8_S4_F4 = 0x31,
        FormatFMask16_S16_F1 = 0x32,
        FormatFMask16_S8_F2 = 0x33,
        FormatFMask32_S16_F2 = 0x34,
        FormatFMask32_S8_F4 = 0x35,
        FormatFMask32_S8_F8 = 0x36,
        FormatFMask64_S16_F4 = 0x37,
        FormatFMask64_S16_F8 = 0x38,
        Format4_4 = 0x39,
        Format6_5_5 = 0x3a,
        Format1 = 0x3b,
        Format1_Reversed = 0x3c,
        Format32_AS_8 = 0x3d,
        Format32_AS_8_8 = 0x3e,
        Format32_AS_32_32_32_32 = 0x3f
    };
    enum class FormatType : uint32_t
    {
        FormatTypeUNorm = 0x0,
        FormatTypeSNorm = 0x1,
        FormatTypeUScaled = 0x2,
        FormatTypeSScaled = 0x3,
        FormatTypeUInt = 0x4,
        FormatTypeSInt = 0x5,
        FormatTypeSNorm_OGL = 0x6,
        FormatTypeFloat = 0x7,
        FormatTypeReserved_8 = 0x8,
        FormatTypeSRGB = 0x9,
        FormatTypeUBNorm = 0xa,
        FormatTypeUBNorm_OGL = 0xb,
        FormatTypeUBInt = 0xc,
        FormatTypeUBScaled = 0xd,
        FormatTypeReserved_14 = 0xe,
        FormatTypeReserved_15 = 0xf
    };
    struct alignas(0x100) Header
    {
        uint32_t gnfMagic{ 0x20464E47U };
        uint32_t imageDataOffset{ 0xF8U };
        uint32_t unk1{ 0x80102U };
        uint32_t fileSize{ 0x200U };
        uint32_t unk2{ 0x0U };
        struct
        {
            uint32_t unk3 : 20{ 0x8 };
            Format format : 6{ Format::Format8 };
            FormatType formatType : 4{ FormatType::FormatTypeUNorm };
            uint32_t unk4 : 2{ 0x0 };
        };
        struct
        {
            uint32_t width : 14 { 0x0 };
            uint32_t height : 14 { 0x0 };
            uint32_t unk5 : 4 { 0x7 };
        };
        struct
        {
            uint32_t destX : 3 { 0x4 };
            uint32_t destY : 3 { 0x0 };
            uint32_t destZ : 3 { 0x0 };
            uint32_t destW : 3 { 0x1 };
            uint32_t unk6 : 4 { 0x0 };
            uint32_t mipmaps : 4 { 0x0 };
            uint32_t unk7 : 12 { 0x96D };
        };
        struct
        {
            uint32_t depth : 13 { 0x0 };
            uint32_t pitch : 13 { 0xF };
            uint32_t unk8 : 6 { 0x0 };
        };
        uint32_t unk9 { 0x0 };
        uint32_t unk10 { 0x0 };
        uint32_t dataSize { 0x100 };
        uint32_t userMagic = 0x52455355U;
        uint32_t unk11 { 0x28 };
        uint64_t userHash { UINT64_MAX };
    };
    class GnfImage
    {
    public:
        Header header;
        std::shared_ptr<byte[]> imageData;
        GnfImage();
        static void UnSwizzle(const byte* src, byte* dest, const uint16_t& w, const uint16_t& h, const uint16_t& bpp, const uint16_t& pixbl);
        static void Swizzle(const byte* src, byte* dst, const uint16_t& w, const uint16_t& h, const uint16_t& bpp, const uint16_t& pixbl);
        static int morton(int t, int sx, int sy);
        void ReadImage(const byte* file);
        void WriteImage(byte*& file);
    };
}