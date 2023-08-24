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

// A System Descriptor Table (SDT). Each SDT starts with a header and then
// expands for (header.length - sizeof(header)) bytes. The header.signature
// indicate the type of the SDT.
struct Sdt {
    RsdtHeader header;
} __attribute__ ((packed));

// Multiple APIC Description Table (MADT). Most likely the most important table,
// contains information about the LAPIC, the I/O APIC(s), the number of CPUs in
// the system and how IRQs are mapped to the I/O APIC(s).
struct Madt {
    RsdtHeader header;
    // Physical address of the Local APIC. All cpus are using this same address
    // to access their own LAPIC.
    u32 localApicPhyAddr;
    // If `1` then indicates that dual 8259 legacy PICs are installed.
    u32 flags;

    // The MADT is then followed by a number of entries (see Entry type below).
    // Each entry contains various info about the interrupts, LAPIC, IO APIC,
    // ...

    // Entry of an Madt. Entries have different sizes depending on their type.
    struct Entry {
        // Type of the entry, for now only list the types of entries we are
        // actually interested in.
        enum class Type : u8 {
            ProcessorLocalApic = 0,
            IoApic = 1,
            InterruptSourceOverride = 2,
            NMISource = 3,
            LocalApicNMI = 4,
            LocalApicAddressOverride = 5,
        };
        // Type of the entry.
        Type type;
        // Length of the entry in bytes.
        u8 length;

        template<typename T>
        T read(u64 const offset) const {
            ASSERT(offset + sizeof(T) <= length);
            VirAddr const addr(this);
            return *(addr + offset).ptr<T>();
        }
    } __attribute__ ((packed));

    // Invoke a lambda on each entry in this MADT. The lambda is expected to
    // take the index of the entry and a pointer to the entry as argument.
    // @param lambda: Function to call on each entry.
    void forEachEntry(void (*lambda)(u64 const, Entry const*)) const {
        VirAddr const start(this);
        VirAddr const end(start + header.length);
        VirAddr curr(start + sizeof(*this));
        u64 index(0);
        while (curr < end) {
            Entry const * const entry(curr.ptr<Entry>());
            lambda(index, entry);
            curr = curr + entry->length;
            index++;
        }
    }
} __attribute__ ((packed));

// Root System Description Table (RSDT). This is essentially a header followed
// by an array of pointers to various System Descriptor Tables (SDT). The size
// of the array is determined by the total length of the RSDT as:
//      Rsdt.header.length - sizeof(RsdtHeader) / 4.
struct Rsdt {
    // The header of the root table.
    RsdtHeader header;
    // Physical addresses to the SDTs.
    u32 sdtPointers[0];

    // Compute the number of tables in the sdtPointers array of this RSDT.
    // @return: The number of SDTs under this RSDT.
    u64 numTables() const {
        return (header.length - sizeof(RsdtHeader)) / 4;
    }

    // Get a pointer to one of the SDTs contained in this RDST.
    // @param index: The index of the table. Must be < numTables().
    // @return: A pointer to the SDT at index i.
    Sdt const * table(u64 const index) const {
        ASSERT(index < numTables());
        PhyAddr const tablePhyAddr(sdtPointers[index]);
        return Paging::toVirAddr(tablePhyAddr).ptr<Sdt const>();
    }
} __attribute__ ((packed));
}
