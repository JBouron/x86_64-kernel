// Definition of the various ACPI tables used in this kernel.
#pragma once

#include <acpi/acpi.hpp>

namespace Acpi {

// FIXME: This file is probably full of undefined behaviour due to having to
// parse structs from raw memory. I should really take the time to double check
// everything here...

// The Root System Description Pointer (RSDP).
struct Rsdp {
    // Signature must be "RSD PTR ", note the space at the end.
    char signature[8];
    // Checksum. Adding all the bytes of the struct including this one should
    // add up to a value for which the lowest byte is 0x0.
    u8 checksum;
    // Set by the EOM, not really important.
    char oemId[6];
    // Revision. If 0 this is ACPI v1.0, if 2 then this is ACPI v2.0 onwards.
    u8 revision;
    // Physical address of the RSDT.
    u32 rsdtAddress;

    // Validate the checksum of this RSDP.
    // @return: true if the checksum is valid, false otherwise.
    bool isValid() const {
        u8 const * ptr(reinterpret_cast<u8 const*>(this));
        u64 sum(0);
        for (u64 i(0); i < sizeof(*this); ++i) {
            sum += ptr[i];
        }
        return !(sum & 0xff);
    }
} __attribute__ ((packed));

// Header for a Root System Description Table (RSDT).
struct RsdtHeader {
    // Signature indicating the type of the table.
    char signature[4];
    // Length of the table in bytes, including this header struct.
    u32 length;
    u8 revision;
    // Checksum byte. Adding all bytes of the table (including this header)
    // should yield a number = 0 (mod 0x100).
    u8 checksum;
    // Some OEM info, unused in this kernel.
    char oemId[6];
    char oemTableId[8];
    u32 oemRevision;
    u32 creatorId;
    u32 creatorRevision;

    // Validate the checksum of this RSDT.
    // @return: true if the checksum is valid, false otherwise.
    bool isValid() const {
        u8 const * ptr(reinterpret_cast<u8 const*>(this));
        u64 sum(0);
        for (u64 i(0); i < length; ++i) {
            sum += ptr[i];
        }
        return !(sum % 0x100);
    }
} __attribute__ ((packed));

// Root System Description Table (RSDT). This is essentially a header followed
// by an array of pointers to various System Descriptor Tables (SDT). The size
// of the array is determined by the total length of the RSDT as:
//      Rsdt.header.length - sizeof(RsdtHeader) / 4.
struct Rsdt {
    // The header of the root table.
    struct RsdtHeader header;
    // Physical addresses to the SDTs.
    u32 sdtPointers[0];

    // Compute the number of tables in the sdtPointers array of this RSDT.
    // @return: The number of SDTs under this RSDT.
    u64 numTables() const {
        return (header.length - sizeof(RsdtHeader)) / 4;
    }
} __attribute__ ((packed));
}
