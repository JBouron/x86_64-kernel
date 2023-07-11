SECTIONS
{
    /* The relocation address for stage 1 which should immediately follow the
     * bootsector/stage 0. */
    . = 0x7e00;

    /* Used to compute the full size of stage 1 + the offset of the entry
     * function within stage 1. */
    STAGE_1_START = .;

    /* The header as expected by stage 0. Obviously must appear first. */
    .header (READONLY) : {
        /* Magic number to identify correct sector. */
        SHORT(0x50f3)
        /* The size of stage 1 in units of sectors. */
        SHORT(STAGE_1_NUM_SECTORS)
        /* Offset of the entry point from 0x7e00. */
        SHORT(stage1Entry - STAGE_1_START)
    }

    /* All text. */
    .text : { *(.text) }

    /* All R/W data. */
    .data : { *(.data) }

    /* The .data.str section contains all the strings used for logging. This is
     * currently the biggest section of all and we make sure that it starts
     * after the .data and .text sections so that we don't end-up with data/code
     * that is inaccessible from 16-bit mode.
     * FIXME: We really need some sort of string dedup, the log macros are
     * bloating the image with duplicate strings, especially PANIC. */
    .data.str (READONLY) : {
        *(.data.str)

        /* This makes sure that the size of stage 1 is a multiple of 512 bytes
         * but only if no section follows this one! */
        . = ALIGN(512);
    }
    STAGE_1_END = .;
    STAGE_1_NUM_SECTORS = (STAGE_1_END - STAGE_1_START) / 512;
}
