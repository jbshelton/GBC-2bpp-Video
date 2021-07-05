/*
GBC 2bpp encoder, version 0.2
DATE: 7/4/2021
Changes: finished semi-HQ encoder
Comments: encoding the audio is difficult :dead_inside:
I still need to get to the video encoding, which is a whole different ordeal
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#define FRAME_SIZE (160 * 120)
#define INPUT_IMG_SIZE (FRAME_SIZE * 3)
#define TILE_DATA_SIZE (127 * 16)
#define FRAME_BLOCK_SIZE ((16384 - TILE_DATA_SIZE) / 13)
#define AUD_VID_SIZE (32 * 15)

static const double sample_rate = 9198.0;
static const double gb_fps = sample_rate / 154.0;
static const double gb_tpf = 1 / gb_fps; //time per frame, used for framerate interpolation

static const size_t pulse_bytes = 308;
static const size_t mv_bytes = 51;
static const size_t total_audio_bytes = pulse_bytes + mv_bytes;
static const size_t audio_offset = 20;
static const size_t palette_bytes = 20;
static const size_t scy_bytes = 120;

static uint8_t output[1024 * 1024 * 8];

size_t main_pos = 0x4000 + TILE_DATA_SIZE + audio_offset; //encoding will always start on offsets of 0x4000 bytes, but audio is encoded before video
size_t mv_pos = 0x4000 + TILE_DATA_SIZE + ((26 * 32) - 12) + 9; //master volume starts at the last 3 bytes of the first 6-byte section of the last 9 sections
//Main audio: 25 12-byte sections, plus 8 bytes / last position: 8th byte of 26th 12-byte section (byte 828)
//Master volume: 4 12-byte sections, plus 3 bytes / first position: 10th byte of 26th 12-byte section (byte 830)

//Note for future player code: decode 24 6-sample segments, decode a 7-sample segment, decode 25 6-sample segments, then another 7-sample segment in an unrolled loop

typedef struct 
{
	unsigned red, green, blue;
}pixel_t;

int main(int argc, const char * argv[]) //eh... I kinda lied, stole this part from GBVideoPlayer2 (only because I'm a total noob at C lol, will change for the final encoder)
{
	if (argc != 5) {
        printf("Usage: %s frame_rate audio_file.raw output.bin\n", argv[0]);
        return -1;
    }
    
    double source_fps = atof(argv[1]);
    double source_tpf = 1 / source_fps; //same as gb_tpf but for source

    double source_time, gb_time, time_diff;

	FILE *audiof = fopen(argv[2], "rb"); //binary read mode (bytes)
    if (!audiof) {
        perror("Failed to load audio source file");
        return 1;
    }

	//THIS CODE WILL NOT WORK WITH CURRENT PROPOSED FORMAT!
	//The format still needs to be finalized, but it certainly isn't neat and tidy like I thought this previous encoder was going to be...
	
    int pulse = 0;
	int outamp = 0;
	int *amplitude_table;
	amplitude_table = malloc(256*8);
	memset(amplitude_table, -1, 256*8);
	//also FYI I pulled most of the below algorithm code from GBAudioPlayerV2's encoder.java and made some small changes
	for(int m=1; m<=8; m++)
	{	
		for(int p=0; p<16; p++)
		{
			if(p<8)
			{
				pulse = p-8;
				pulse = (pulse*2)+1;
				outamp = (int)(128.0+((128.0/120.0)*((double)(pulse*m))));
				amplitude_table[outamp+(256*(m-1))] = p;
			}
			else
			{
				pulse = p-7;
				pulse = (pulse*2)-1;
				outamp = (int)(127.0+((128.0/120.0)*((double)(pulse*m))));
				amplitude_table[outamp+(256*(m-1))] = p;
			}
		}
	}
	//free(left_amplitude_table);
	//free(right_amplitude_table);
	//now that the tables have been generated, do an outside-in value spread (if the current array value is -1, replace it with the last read positive value, otherwise read the value and move on)
	int sample_sort = -1;
	for(int m=1; m<=8; m++)
	{	
		for(int i=0; i<128; i++)
		{
			if(amplitude_table[i+(i*(m-1))]!=-1)
			{
				sample_sort = amplitude_table[i+(i*(m-1))];
			}
			if(sample_sort!=-1)
			{
				amplitude_table[i+(i*(m-1))] = sample_sort;
			}
		}
		sample_sort = -1;
		for(int i=255; i>127; i--)
		{
			if(amplitude_table[i+(i*(m-1))]!=-1)
			{
				sample_sort = amplitude_table[i+(i*(m-1))];
			}
			if(sample_sort!=-1)
			{
				amplitude_table[i+(i*(m-1))] = sample_sort;
			}
		}
	}
	//now do the process in reverse to make sure there are no -1s left in the array
	sample_sort = -1;
	for(int m=1; m<=8; m++)
	{
		for(int i=127; i>=0; i--)
		{
			if(amplitude_table[i+(i*(m-1))]!=-1)
			{
				sample_sort = amplitude_table[i+(i*(m-1))];
			}
			if(sample_sort!=-1)
			{
				amplitude_table[i+(i*(m-1))] = sample_sort;
			}
		}
		sample_sort = -1;
		for(int i=128; i<=255; i++)
		{
			if(amplitude_table[i+(i*(m-1))]!=-1)
			{
				sample_sort = amplitude_table[i+(i*(m-1))];
			}
			if(sample_sort!=-1)
			{
				amplitude_table[i+(i*(m-1))] = sample_sort;
			}
		}
	}

	bool done = false;
	uint8_t left_aud_batch[6];
	uint8_t left_aud_ext_batch[7];
	uint8_t right_aud_batch[6];
	uint8_t right_aud_ext_batch[7];
	uint8_t left_pulses[6];
	uint8_t right_pulses[6];
	uint8_t left_ext_pulses[7];
	uint8_t right_ext_pulses[7];
	uint8_t left_mv, right_mv;
    while(!done || pos<sizeof(output)) 
    {
        //49 6-sample segments, 2 7-sample segments, per 360-byte audio section
        //record 24 6-sample semi-HQ segments, record a 7-sample segment, then 25 6-sample segments, then another 7-sample segments
        //check the main position and mv position with if statements to compensate for the offset-
        //if the position isn't between 20 and 31 in the block, reset block position and increment by 1
        for(unsigned i=0; i<24; i++) 
        {	
            for(unsigned r=0; r<6; r++)
            {	
            	if(fread(&left_aud_batch[r], 1, 1, audiof) != 1 || fread(&right_aud_batch[r], 1, 1, audiof) != 1) 
            	{
                	left_aud_batch[r] = right_aud_batch[r] = 0x80;
                	done = true;
            	}
            }
            semi_hq_encode(false, &left_aud_batch, &right_aud_batch, amplitude_table, &left_pulses, &right_pulses, &left_mv, &right_mv);
            for(unsigned r=0; r<6; r++)
            {
            	output[main_pos++] = ((left_pulses[r]<<4)&0xf0)|(right_pulses[r]&0x0f);
            }
            output[mv_pos++] = ((left_mv<<4)&0xf0)|(right_mv&0x0f);
            main_pos &= ~0x001f; //scratch the lower 5 bits (32 bytes)
            main_pos += 0x1f; //increment by 32 bytes
        }
        for(unsigned r=0; r<7; r++)
        {
        	if(fread(&left_aud_ext_batch[r], 1, 1, audiof) != 1 || fread(&right_aud_ext_batch[r], 1, 1, audiof) != 1) 
            {
               	left_aud_ext_batch[r] = right_aud_ext_batch[r] = 0x80;
               	done = true;
            }
        }
        semi_hq_encode(true, &left_aud_ext_batch, &right_aud_ext_batch, amplitude_table, &left_ext_pulses, &right_ext_pulses, &left_mv, &right_mv);
        
        pos += (block_size - audio_size);
    }
    done = false;
    pos = 0x4000;
    fclose(audiof);

    uint8_t input_img[INPUT_IMG_SIZE];
    uint8_t output_img[OUTPUT_IMG_SIZE];
    uint8_t out_tiled[OUTPUT_IMG_SIZE];
    char in_filepath[1024];
    char out_filepath[1024];
    scanf("%s", out_filepath); //modify the makefile to output the directory of the output file first!
    FILE *out_img = fopen(out_filepath, "wb"); //encodes all the images as one big ass file, might change to a frame list and individual file-based method (I'll make another document for output/input files)
    while(true)
    {
    	gb_time += gb_tpf;
    	source_time += source_tpf;
    	if(gb_tpf>source_tpf)
    	{
    		if(gb_time-source_time>source_tpf)
    		{
    			source_time += source_tpf;
    			time_diff = source_time-gb_time;
    			source_time = time_diff;
    			gb_time = 0.0;
    			blend_and_repeat_frames(in_filepath, in_img, out_img); //writes a blend of the current frame and next frame, then the next frame
    			fclose(in_img);
    		}
    		else
    		{
    			//encode frame normally
    			scanf("%1023s", in_filepath);
	    		FILE *in_img = fopen(in_filepath, "rb");
    			fread(&input_img, 1, INPUT_IMG_SIZE, in_img);
    			conv_frame_to_grayscale(&input_img, &output_img);
    			tileformat_image(&output_img, &out_tiled);
    			fwrite(&out_tiled, 1, OUTPUT_IMG_SIZE, out_img);
    			fclose(in_img);
    		}
    	}
    	if(source_tpf>gb_tpf) //if the video happens to be 30fps then this will come in handy (but bad apple is 60fps so I probably won't test this) (I will actually encode and submit Bad Apple for the GB Compo)
    	{
    		if(source_time-gb_time>gb_tpf)
    		{
    			gb_time += gb_tpf;
    			time_diff = gb_time-source_time;
    			gb_time = time_diff;
    			source_time = 0.0;
    			blend_and_repeat_frames(in_filepath, in_img, out_img);
    			fclose(in_img);
    		}
    		else
    		{
    			//encode frame normally
    			scanf("%1023s", in_filepath);
	    		FILE *in_img = fopen(in_filepath, "rb");
    			fread(&input_img, 1, INPUT_IMG_SIZE, in_img);
    			conv_frame_to_grayscale(&input_img, &output_img);
    			tileformat_image(&output_img, &out_tiled);
    			fwrite(&out_tiled, 1, OUTPUT_IMG_SIZE, out_img);
    			fclose(in_img);
    		}
    	}
    	else
    	{
    		//if the two framerates miraculously match exactly, just encode stuff as normal (highly doubt this will get used)
    		scanf("%1023s", in_filepath);
	    	FILE *in_img = fopen(in_filepath, "rb");
    		fread(&input_img, 1, INPUT_IMG_SIZE, in_img);
    		conv_frame_to_grayscale(&input_img, &output_img);
    		tileformat_image(&output_img, &out_tiled);
    		fwrite(&out_tiled, 1, OUTPUT_IMG_SIZE, out_img);
    		fclose(in_img);
    	}
    }
    fclose(out_img);
    FILE *tile_source = fopen(out_filepath, "rb");
}

void tileformat_image(uint8_t * input_image[], uint8_t * output_tiles[])
{
	//fy = frame y
	//fx = frame x
	//ty = tile y
	//tx = tile x
	for(size_t fy=0; fy<15; fy++)	
	{
		for(size_t fx=0; fx<20; fx++)
		{	
			for(size_t ty=0; ty<8; ty++)
			{
				for(size_t tx=0; tx<2; tx++)
				{
					output_tiles[(fy*320)+(fx*16)+(ty*2)+tx] = input_image[(fy*320)+(fx*2)+(ty*40)+tx];
					//input:
					//tx = byte offset
					//ty = each column is 2 bytes wide, 20 columns (40 bytes offset total)
					//fx = kinda same as ty
					//fy = 15 blocks of 320 bytes

					//output: multiplier is bytes
					//tx = byte offset
					//ty = 2 bytes per tile x, 8 rows (16 bytes total)
					//fx = 20 blocks of 16 bytes (320 bytes total)
					//fy = 15 blocks of 320 bytes

					//final format: each group of 16 bytes is represented as a tile
					//as of 7/3/2021 yeah I forget what I was going for here
				}
			}
		}
	}
}

//disclaimer: I don't know how returning an array/pointer works in C since I come from Java so this might look kinda janky
//subject to change once I know how to get the size of arrays and return an array from a function
void semi_hq_encode(bool extra_sample, uint8_t * left_sample_list[], uint8_t * right_sample_list[], int * semi_hq_table[], uint8_t * left_results[], uint8_t * right_results[], uint8_t * left_mv_out, uint8_t * right_mv_out)
{
	uint8_t largest_left_sample = 0;
	uint8_t largest_right_sample = 0;
	size_t sample_batch_size = 6;
	if(extra_sample==true)
	{
		sample_batch_size = 7;
	}
	for(size_t i=0; i<sample_batch_size; i++)
	{
		if(left_sample_list[i]>127)
		{
			if((left_sample_list[i]-127) > largest_left_sample)
			{
				largest_left_sample = left_sample_list[i];
			}
		}
		else
		{
			if((127-left_sample_list[i]) > largest_left_sample)
			{
				largest_left_sample = left_sample_list[i];
			}
		}
		if(right_sample_list[i]>127)
		{
			if((right_sample_list[i]-127) > largest_right_sample)
			{
				largest_right_sample = right_sample_list[i];
			}
		}
		else
		{
			if((127-right_sample_list[i]) > largest_right_sample)
			{
				largest_right_sample = right_sample_list[i];
			}
		}
	}
	//now to figure out what master volume to use by comparing the largest sample to the largest master volume multiplier values
	bool last_mv_too_big = true;
	uint8_t left_mv = 0;
	uint8_t right_mv = 0;
	for(uint8_t m=8; m>0; m--)
	{
		if(((uint8_t)((128.0/120.0)*(15.0*(double)m))-1) > largest_left_sample)
		{
			if(last_mv_too_big!=true)
			{
				left_mv = m;
				break;
			}
			else
			{
				last_mv_too_big = true;
			}
		}
		else
		{
			left_mv = m-1;
			break;
		}
	}
	last_mv_too_big = true;
	for(uint8_t m=8; m>0; m--)
	{
		if(((uint8_t)((128.0/120.0)*(15.0*(double)m))-1) > largest_right_sample)
		{
			if(last_mv_too_big!=true)
			{
				right_mv = m;
				break;
			}
			else
			{
				last_mv_too_big = true;
			}
		}
		else
		{
			right_mv = m-1;
			break;
		}
	}
	for(int i=0; i<sample_batch_size; i++)
	{	
		left_results[i] = semi_hq_table[left_sample_list[i]+(left_mv*256)];
		right_results [i] = semi_hq_table[right_sample_list[i]+(right_mv*256)];
	}
	left_mv_out = left_mv;
	right_mv_out = right_mv;
}

void conv_frame_to_grayscale(uint8_t *input_image, uint8_t *output_image)
{
	pixel_t pixel_batch[4];
	size_t in_img_pos = 0;
	size_t out_img_pos = 0;
	while(in_img_pos<INPUT_IMG_SIZE)
	{	
		for(size_t i=0; i<4; i++)
		{
			pixel_batch[i].blue = input_image[in_img_pos++];
			pixel_batch[i].green = input_image[in_img_pos++];
			pixel_batch[i].red = input_image[in_img_pos++];
		}
		conv_pix_to_grayscale(pixel_batch, output_image[out_img_pos++]);
	}
}

//dunno what I'm gonna do with this, might pitch the whole pixel batch thing entirely
void conv_pix_to_grayscale(pixel_t pixel_batch, uint8_t *output_byte) //expects pixel batch to be 4 pixels large
{
	for(size_t i=0; i<3; i++)
	{
		output_byte |= ((pixel_batch[i].red + pixel_batch[i].green + pixel_batch[i].blue)/3) & 0xC0;
		output_byte = output_byte >> 2;
	}
	output_byte |= (uint8_t)((pixel_batch[3].red + pixel_batch[3].green + pixel_batch[3].blue)/3) & 0xC0;
}

//if somebody's actually reading this, would you recommend averaging frames, ORing them, or not blending them at all and simply repeating them?
void blend_and_repeat_frames(char in_filepath, FILE *in_frame, FILE *out_frame)
{
	char this_filepath = in_filepath;
	uint8_t input_image[INPUT_IMG_SIZE], output_image_0[OUTPUT_IMG_SIZE], output_image_1[OUTPUT_IMG_SIZE], blended_image[OUTPUT_IMG_SIZE];
	fread(&input_image, 1, INPUT_IMG_SIZE, in_frame);
	conv_frame_to_grayscale(&input_image, &output_image_0);
	fclose(in_frame);
	scanf("%1023s", this_filepath);
	FILE *in_frame_0 = fopen(this_filepath, "rb")
	fread(&input_image, 1, INPUT_IMG_SIZE, in_frame);
	conv_frame_to_grayscale(&input_image, &output_image_1);
	fclose(in_frame_0);
	for(unsigned i=0; i<OUTPUT_IMG_SIZE; i++)
	{
		blended_image[i] |= (((output_image_0[i]>>6)&0x03 + (output_image_1[i]>>6)&0x03)/2)<<6;
		blended_image[i] |= (((output_image_0[i]>>4&0x03) + (output_image_1[i]>>4)&0x03)/2)<<4;
		blended_image[i] |= (((output_image_0[i]>>2)&0x03 + (output_image_1[i]>>2)&0x03)/2)<<2;
		blended_image[i] |= ((output_image_0[i]&0x03 + output_image_1[i]&0x03)/2);
	}
	uint8_t tiled_image[OUTPUT_IMG_SIZE];
	tileformat_image(&blended_image, &tiled_image);
	fwrite(&tiled_image, 1, OUTPUT_IMG_SIZE, out_frame);
	tileformat_image(&output_image_1, &tiled_image);
	fwrite(&tiled_image, 1, OUTPUT_IMG_SIZE, out_frame);
}

//the only thing I MIGHT use from this is the image-to-tile format conversion
//otherwise, I think the tile list generation from whatever I'd put here is pretty useless
void generate_tile_list(uint8_t *input_image, uint8_t *output_data, int *tile_indexes)
{
	for(size_t i=0; i<300; i++)
	{
		tile_indexes[i] = -1;
	}
	uint8_t tile_0[16];
	uint8_t tile_1[16];
	size_t tile_index = 0;
	bool same_tile = true;	
	for(size_t t0=0; t0<15; t0++)
	{	
		for(size_t r0=0; r0<20; r0++)
		{
			for(size_t c0=0; c0<16; c0++)
			{
				tile_0[c0] = input_image[c0+(r0*16)+(t0*320)];
			}
			if(tile_indexes[r0+(t0*20)]==-1)
			{	
				tile_indexes[r0+(t0*20)] = tile_index; 
				//don't increment tile index yet! We still need to find out if there's repeats of this tile.
				//we don't increment the tile index until all repeats of one tile have been stored
			}
			for(size_t t1=0; t1<15; t1++)	
			{
				for(size_t r1=0; r1<20; r1++)
				{
					same_tile = true;
					for(size_t c1=0; c1<16; c1++)
					{
						tile_1[c1] = input_image[c1+(r1*16)+(t1*320)];
						if(tile_0[c1]!=tile_1[c1])
						{
							same_tile = false;
						}
					}
					if(same_tile!=false && tile_indexes[r1+(t1*20)]==-1)
					{
						//store repeated tile index in list without incrementing the index
						//only if the contents have not been filled before! otherwise this could overwrite the index with the 
						//incorrect one that's one more than it's supposed to be
						tile_indexes[r1+(t1*20)] = tile_index;
					}
				}
			}
			//end of checking repeats, increment the tile index
			//without extra checks this could cause the tile index array to be overwritten with an incorrect index
			tile_index++;
		}
	}
}