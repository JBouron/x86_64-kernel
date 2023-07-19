// Everything related to segmentation.
// Despite being mostly disabled in 64-bit mode, we still need to setup the GDT.

#pragma once

namespace Memory::Segmentation {

// Initialize segmentation. Create a GDT and load it in GDTR.
void Init();

}
