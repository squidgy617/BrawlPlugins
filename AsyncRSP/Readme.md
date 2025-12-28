# Memory Expansion

Uncomment the line `#define MEMORY_EXPANSION` in both css_hooks.cpp and sel_char_load_thread.cpp to enable memory expansion. Expanded memory allows larger BRRES archives by using the FighterXResource heaps instead of MenuResource to load them. It REQUIRES the code `Don't load fighter files on CSS [MarioDox]`.

For expanded RSPs to work on results screen, you must also update the `MemoryAllocation` field in pf/stage/stageinfo/Results.param to `0x00380000`.