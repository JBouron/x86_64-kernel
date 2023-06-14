SECTIONS
{
    /* The relocation address for stage 1 which should immediately follow the
     * bootsector/stage 0. */
    . = 0x7e00;

    /* Used to compute the full size of stage 1 + the offset of the entry
     * function within stage 1. */
    STAGE_1_START = .;

    /* The header as expected by stage 0. Obviously must appear first. */
    .header : { 
        /* Magic number to identify correct sector. */
        SHORT(0x50f3)
        /* The size of stage 1 in units of sectors. */
        SHORT(STAGE_1_NUM_SECTORS)
        /* Offset of the entry point from 0x7e00. */
        SHORT(stage1Entry - STAGE_1_START)
    }
    /* .text and .data sections from all input files. */
    .text : { *(.text) }
    .data : {
        *(.data)

        /* This makes sure that the size of stage 1 is a multiple of 512 bytes
         * but only if no section follows this one! */
        . = ALIGN(512);
    }
    STAGE_1_NUM_SECTORS = (. - STAGE_1_START) / 512;
}
