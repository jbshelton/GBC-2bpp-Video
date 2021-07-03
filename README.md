# GBC-2bpp-Video
A 2 bits per pixel video encoder (with audio) for the Gameboy Color, entry for the 2021 GB Compo (as a demo.)
I will be changing this file very often, as it will be a reference for myself for both making the encoder and the ASM player!
THIS VERSION: 7/3/2021

---

### Rules/expectations for encoder/player:
- "Optimized" to use as much space in an 8MiB (MBC5) cartridge as possible, without wasting space
- Will not use any existing code base except snippets from GBAudioPlayerV2 (for audio)
- The actual encoder will be coded in C (I don't really know C, so this will be a learning experience for me!)
- NOTE: the encoder will be optimized for dealing with audio and video that is 3 minutes and 40 seconds long or less

---

### General specifications
- Resolution: 160x120 (4:3)
- Framerate: 59.727 frames per second
- Video format: 2 bits per pixel (4-value grayscale)
- Audio format: proprietary semi-high-quality stereo audio at 9.198KHz

---

### Constraints/limitations of the system
- Can't put full HQ audio, even with 8MiB of cartridge space (great explanation of HQ audio available in my GBAudioPlayerV2 repository!)
- Some compression is done which may result in stray pixels and slightly inaccurate frames, as well as slow encoding speeds
- Timing to transfer video data from the cartridge to VRAM will be very tight, since I can't disable the LCD to have access to VRAM while the screen is still syncing (the entire PPU is disabled, and the frame after enabling the LCD is blank!!!)

---

### Final data format/usage
- 1,102 bytes per block of 2 frames
- 308 bytes used for 4-bit audio (154 bytes per frame)
- 51 bytes used for semi-HQ audio addon (master volume register is changed every 6th 4-bit sample)
- 600 bytes used for 20x15 tile and attribute maps (same maps for both frames)
- 120 bytes used for SCY changes (same pattern for both frames)
- 16 bytes used for color palettes (8 bytes per frame, 1 byte per palette, 2 bits per color)
- NOTE: since DMA transfers ignore address bits 0-3, the tile and attribute maps will have to be padded to 16-byte offsets, most likely by using some audio data (this won't be a problem for the 128 tiles, since they will always be at the very beginning of each bank)

---

### The compression
- Two encoded 2bpp frames will be combined into a 4bpp frame, having 24 different "colors" per pixel
- 16 of those "colors" represent overlapping combinations from both of the frames
- To squeeze 2 frames out of a single 20x15 tile map and attribute map, the color palettes are changed every frame, very similar to how some animations were compressed in Sonic 3D Blast (see the "Sonic 3D's Impossibly Compressed Logo FMV" video from GameHut as reference)
- To increase image quality, SCY is changed every line, effectively allowing an on-screen tile (really 8 tiles just sliced up) to use all 8 color palettes

---

### Encoding process
1. Group of 26 frames are read and converted to 2 bits per pixel grayscale
2. Groups of 2 frames are combined into one 24-color image
3. Horizontal lines from all COMBINED images are scanned and groups of 8 pixels are quantized to 4 colors per section, palettes are tested for each section to match the INDIVIDUAL frames, and those palettes are checked/added to a list of most used palettes PER FRAME
4. Encoder checks/truncates/optimizes the palettes and images so that only the top 8 palettes are used PER FRAME
5. Encoder makes sure that the number of lines that get a similar attribute string in a COMBINED image is divisible by 8 (if less than 4 over, truncate; if more than 4 over, find remaining lines that could use the same attribute string)
6. Each COMBINED image is searched for groups of 8 lines with the same attribute/palette string, and a duplicate image is encoded with the lines in that order, along with the respective Y coordinate of the line
7. The final SCY values are calculated using a Y scroll counter to emulate the Gameboy combined with the Y coordinates of where the correctly ordered lines are
8. The newly-encoded image data is then read out, and the encoder optimizes the tile data and attribute strings so that all 13 combined images use a maximum of 128 tiles together
9. If duplicate checking with the flipping attributes still doesn't reduce the tile count to 128 or less, replacement tiles that are close enough will be allocated and the affected COMBINED images will be re-encoded in a separate output image, and how close it is to the intended image will be compared with the image that did not get a second encoding pass

---

### CPU scheduling
- Transfer 128 tiles, each 0x10 bytes large, to VRAM at the beginning of each bank (each byte takes 1 M-cycle, so all tiles take 2048 M-cycles, or 8,192 T-cycles- which means that it should be done in Vblank)
- Transfer 300 bytes of tile map data to VRAM bank 0
- Transfer 300 bytes of tile attribute data to VRAM bank 1
- For transferring tile map and attribute data: 300 bytes each makes for a transfer length of 19 0x10-byte blocks, which is 304 M-cycles, or 1,216 T-cycles
- Time all transfers so that the audio will play when there is a horizontal blanking interrupt (or because of hblank DMA transfers for the maps, have a dummy stall mechanism to match that delay so the audio won't get distorted)
- Total max time to transfer data for a frame: 2,656 M-cycles (10,624 T-cycles)
- There are 9,120 total available T-cycles (or 2,280 M-cycles) available during the vertical blanking interval, but at least 25% of that will be used for playing back audio and writing to SCY
- About 11 tiles can be transferred per line in the vertical blanking interval before the audio and SCY interrupt, so if the transfer really can't get done in a single frame, then the first combined frame of the bank will have to be optimized to use 64 tiles so the transfer can complete on the next frame. This allows more horizontal blanking time to be used, as only 8 tiles per line have to be transferred, over 8 lines.
- Since the resolution is 160x120, there are 24 horizontal blanking intervals to transfer data before any image data starts displaying, so the last 2 vertical blanking lines, as well as the horizontal interrupts, can be used to transfer the tile map and tile attributes. If just 32 of the bytes are transferred each interrupt (128 T-cycles,) all of that data will be transferred in just 19 horizontal blanking intervals, which leaves enough time to write the palettes.
- In order to utilize all the time during hblank to do DMA transfers, I will need to experiment with using the timer in order to schedule updating the audio (at the horizontal scan rate) instead of the horizontal blanking interrupt.

---

### Other notes
- I may or may not rickroll people with this when I finish it
- Because the player doesn't have any specific monochrome decoding/decompression aspects other than the smaller palette, I may make a version that encodes 32-color video at half the framerate
