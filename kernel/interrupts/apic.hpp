// Functions and types related to the Advanced Programmable Interrupt Controller
// (APIC).
#pragma once

namespace Interrupts {
namespace Apic {

// Initialize the APIC.
void Init();

// Notify the local APIC of the End-Of-Interrupt.
void eoi();

}
}
