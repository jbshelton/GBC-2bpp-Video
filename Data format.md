# Encoded data format
Because the tile resolution is 20x15, and not a multiple of 16, I'll reference this document when writing the encoder and player to format the data on the cartridge as efficiently as possible.

THIS VERSION: 7/4/2021

---

### Tile/attribute map and audio format
In order to transfer the tile and attribute maps to VRAM most efficiently, I have to pad the last 12 bytes of a row of tiles with some data. This data will be audio data.
This means that the maps for a 2-frame group together take up 32x30 bytes- 960 in total. That leaves 360 bytes for the audio data. Since the 308 bytes for the audio data isn't exactly divisible by 12, and the 51 bytes of semi-HQ data isn't either, some unrolled loops/code will have to be used to decode the audio data without wasting CPU cycles on conditional jumps. Here's a better explanation/list of this format:
- 49 6-byte sections of stereo 4-bit audio data (294 bytes), plus an extra 2 sections to accomodate for 7-byte strings, with 2 bytes overflowing into the 52nd section (26 12-byte sections)
- 8 6-byte sections of master volume data, with the first 3 bytes being in the section with the overflowed 4-bit data (4 12-byte sections)

Miraculously, this brings the byte usage to 357, which is nearly 100% efficient! The remaining 3 bytes can be blank, because it can't really be used anywhere else.

---

### Color palette and SCY change pattern format
The color palettes take up 20 bytes, and the SCY values take up 120 bytes, which brings this section to 140 bytes. The order they're in doesn't matter that much, just the size. Thankfully, it's only 4 bytes away from 144, which is a multiple of 16 bytes (because the DMA requires that data be in offsets of 16 bytes.)
This brings the total 2-frame block size to 1,104 bytes. Fortunately, this only brings the size of the tile section at the beginning of each bank down by 1 single tile- for a total of 127 unique tiles per bank. I could use some of the 91 remaining stray bytes to squeeze in that last tile, but I don't think it'll impact quality that much, and it will take up more CPU cycles.

---

### Last note
Because the audio data overlaps with the tile map data a bit, some of the audio will be DMA'd to VRAM. I can't wait to see what it looks like in BGB (^:
