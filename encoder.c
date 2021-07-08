/*
GBC 2bpp encoder, version 0.3
DATE: 7/7/2021
Changes: ACTUALLY finished semi-HQ encoder, made debug output encoder (^:
Comments: encoding the audio is difficult :dead_inside:
I still need to get to the video encoding, which is a whole different ordeal
TODO: build video encoder from the ground up, the old code is useless
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#define TILE_DATA_SIZE (127 * 16)

//#define FRAME_SIZE (160 * 120)
//#define INPUT_IMG_SIZE (FRAME_SIZE * 3)

//#define FRAME_BLOCK_SIZE ((16384 - TILE_DATA_SIZE) / 13)
//#define AUD_VID_SIZE (32 * 15)

void semi_hq_encode(bool extra_sample, uint8_t *left_sample_list, uint8_t *right_sample_list, uint8_t *semi_hq_table, uint8_t *left_results, uint8_t *right_results, uint8_t *left_mv_out, uint8_t *right_mv_out)
{

	uint8_t largest_left_sample = 0;
	uint8_t largest_right_sample = 0;
	uint8_t right_pulse = 0;
	uint8_t left_pulse = 0;
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
			if((128-left_sample_list[i]) > largest_left_sample)
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
			if((128-right_sample_list[i]) > largest_right_sample)
			{
				largest_right_sample = right_sample_list[i];
			}
		}
	}

	//now to figure out what master volume to use by converting the largest samples to GB-equivalent range and dividing that by the largest equivalent pulse amplitude
	uint8_t left_mv = (uint8_t)(ceil((((double)largest_left_sample)*(120.0/128.0))/15.0))-1;
	uint8_t right_mv = (uint8_t)(ceil((((double)largest_right_sample)*(120.0/128.0))/15.0))-1;

	for(int i=0; i<sample_batch_size; i++)
	{	
		if(left_sample_list[i]>127)
		{
			left_pulse = ((uint8_t)(ceil(((double)(left_sample_list[i]-127)*(120.0/128.0))/((double)left_mv+1)))+1)/2;
			if(left_pulse>8)
			{
				left_pulse = 8;
			}
			left_results[i] = left_pulse + 7;
		}
		else
		{
			left_pulse = ((uint8_t)(ceil(((double)(128-left_sample_list[i])*(120.0/128.0))/((double)left_mv+1)))+1)/2;
			if(left_pulse>8)
			{
				left_pulse = 8;
			}
			left_results[i] = 8 - left_pulse;
		}
		if(right_sample_list[i]>127)
		{
			right_pulse = ((uint8_t)(ceil(((double)(right_sample_list[i]-127)*(120.0/128.0))/((double)right_mv+1)))+1)/2;
			if(right_pulse>8)
			{
				right_pulse = 8;
			}
			right_results[i] = right_pulse + 7;
		}
		else
		{
			right_pulse = ((uint8_t)(ceil(((double)(128-right_sample_list[i])*(120.0/128.0))/((double)right_mv+1)))+1)/2;
			if(right_pulse>8)
			{
				right_pulse = 8;
			}
			right_results[i] = 8 - right_pulse;
		}
		
	}
	*left_mv_out = left_mv;
	*right_mv_out = right_mv;
}

void reverse_audio(bool extra_sample, uint8_t *left_pulses, uint8_t *right_pulses, uint8_t left_mv, uint8_t right_mv, size_t *out_pos, uint8_t *out_arr)
{
	int pulse = 0;
	int batch_size = 6;
	if(extra_sample==true)
	{
		batch_size = 7;
	}
	for(int i=0; i<batch_size; i++)
	{	
		if(left_pulses[i]<8)
		{
			pulse = (int)left_pulses[i] - 8;
			pulse = (pulse*2)+1;
			out_arr[*out_pos] = (uint8_t)(128.0+((128.0/120.0)*((double)(pulse*((int)left_mv+1)))));
		}
		else
		{
			pulse = (int)left_pulses[i] - 7;
			pulse = (pulse*2)-1;
			out_arr[*out_pos] = (uint8_t)(127.0+((128.0/120.0)*((double)(pulse*((int)left_mv+1)))));
		}
		(*out_pos)++;
		if(right_pulses[i]<8)
		{
			pulse = (int)right_pulses[i] - 8;
			pulse = (pulse*2)+1;
			out_arr[*out_pos] = (uint8_t)(128.0+((128.0/120.0)*((double)(pulse*((int)right_mv+1)))));
		}
		else
		{
			pulse = (int)right_pulses[i] - 7;
			pulse = (pulse*2)-1;
			out_arr[*out_pos] = (uint8_t)(127.0+((128.0/120.0)*((double)(pulse*((int)right_mv+1)))));
		}
		(*out_pos)++;
	}
}

int main(int argc, const char * argv[]) //eh... I kinda lied, stole this part from GBVideoPlayer2 (only because I'm a total noob at C lol, will change for the final encoder)
{
	if (argc != 5) {
        printf("Usage: %s frame_rate audio_file.raw output.bin\n", argv[0]);
        return -1;
    }
    
    printf("The encoder is executing \n");
    //double source_fps = atof(argv[1]);
    //double source_tpf = 1 / source_fps; //same as gb_tpf but for source

    //double source_time, gb_time, time_diff;

	FILE *audiof = fopen(argv[2], "rb"); //binary read mode (bytes)
    if (!audiof) {
        perror("Failed to load audio source file");
        return 1;
    }
	
    //static const double sample_rate = 9198.0;
	//static const double gb_fps = sample_rate / 154.0;
	//static const double gb_tpf = 1 / gb_fps; //time per frame, used for framerate interpolation

	//static const size_t pulse_bytes = 308;
	//static const size_t mv_bytes = 51;
	//static const size_t total_audio_bytes = pulse_bytes + mv_bytes;
	//static const size_t palette_bytes = 20;
	//static const size_t scy_bytes = 120;

	static const size_t audio_offset = 20;
	static const size_t block_offset = 1104;
	static const size_t base_mv_offset = ((25 * 32) + 9); //master volume starts at the last 3 bytes of the first 6-byte section of the last 9 sections 
	//(todo: fix this, the output file's offset isn't what I expect it to be?)
	size_t main_pos = 0x4000 + TILE_DATA_SIZE; //encoding will always start on offsets of 0x4000 bytes, but audio is encoded before video
	size_t debug_pos = 0;
	size_t main_block_pos = audio_offset;
	size_t mv_block_pos = base_mv_offset;

	static uint8_t output[1024 * 1024 * 8];
	static uint8_t debug_audio[1024 * 1024 * 8];
	memset(debug_audio, 0x80, sizeof(debug_audio));
	static uint8_t block[1104];

	//Main audio: 25 12-byte sections, plus 8 bytes / last position: 8th byte of 26th 12-byte section (byte 827, zero-indexed) (hex $33B)
	//Master volume: 4 12-byte sections, plus 3 bytes / first position: 10th byte of 26th 12-byte section (byte 829, zero-indexed) (hex $33D)

	//Note for future player code: decode 24 6-sample segments, decode a 7-sample segment, decode 25 6-sample segments, then another 7-sample segment in an unrolled loop

	printf("Initial crap has been defined \n");

    int pulse = 0;
	uint8_t outamp = 0;
	uint8_t amplitude_table[8 * 256];
	for(int i=0; i<8; i++)
	{
		for(int j=0; j<256; j++)
		{
			amplitude_table[(i*256) + j] = -1;
		}
	}
	
	printf("Segmentation fault did not occur at 2D quantization array \n");

	printf("The amplitude table has been allocated \n");
	//also FYI I pulled most of the below algorithm code from GBAudioPlayerV2's encoder.java and made some small changes
	for(uint8_t m=1; m<=8; m++)
	{	
		for(uint8_t p=0; p<16; p++)
		{
			if(p<8)
			{
				pulse = (int)p-8;
				pulse = (pulse*2)+1;
				outamp = (uint8_t)(128.0+((128.0/120.0)*((double)(pulse*(int)m))));
				amplitude_table[((m-1)*256)+outamp] = p; //had to index a 1D array because 2D ones were being a painus in the anus ):<
			}
			else
			{
				pulse = (int)p-7;
				pulse = (pulse*2)-1;
				outamp = (uint8_t)(127.0+((128.0/120.0)*((double)(pulse*(int)m))));
				amplitude_table[((m-1)*256)+outamp] = p;
			}
		}
	}
	//now that the tables have been generated, do an outside-in value spread (if the current array value is -1, replace it with the last read positive value, otherwise read the value and move on)
	int sample_sort = -1;
	for(int m=0; m<8; m++)
	{	
		for(int i=0; i<128; i++)
		{
			if(sample_sort!=-1 && amplitude_table[(m*256)+i]==-1)
			{
				amplitude_table[(m*256)+i] = sample_sort;
			}
			if(amplitude_table[(m*256)+i]!=-1)
			{
				sample_sort = amplitude_table[(m*256)+i];
			}
		}
		sample_sort = -1;
		for(int i=255; i>127; i--)
		{
			if(sample_sort!=-1 && amplitude_table[(m*256)+i]==-1)
			{
				amplitude_table[(m*256)+i] = sample_sort;
			}
			if(amplitude_table[(m*256)+i]!=-1)
			{
				sample_sort = amplitude_table[(m*256)+i];
			}
		}
	}
	//now do the process in reverse to make sure there are no -1s left in the array
	sample_sort = -1;
	for(int m=0; m<8; m++)
	{
		for(int i=127; i>=0; i--)
		{
			if(sample_sort!=-1 && amplitude_table[(m*256)+i]==-1)
			{
				amplitude_table[(m*256)+i] = sample_sort;
			}
			if(amplitude_table[(m*256)+i]!=-1)
			{
				sample_sort = amplitude_table[(m*256)+i];
			}
		}
		sample_sort = -1;
		for(int i=128; i<=255; i++)
		{
			if(sample_sort!=-1 && amplitude_table[(m*256)+i]==-1)
			{
				amplitude_table[(m*256)+i] = sample_sort;
			}
			if(amplitude_table[(m*256)+i]!=-1)
			{
				sample_sort = amplitude_table[(m*256)+i];
			}
		}
	}

	printf("Amplitudes have been sorted \n");

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
	int j, i, r;
	
	printf("Right before entering loop \n");

	if((main_pos&0x1f)==0)
	{
		printf("You fucked up \n");
	}

    while((!done) && (main_pos<sizeof(output))) 
    {
        //49 6-sample segments, 2 7-sample segments, per 360-byte audio section
        //record 24 6-sample semi-HQ segments, record a 7-sample segment, then 25 6-sample segments, then another 7-sample segments
        //check the main position and mv position with if statements to compensate for the offset-
        //if the position isn't between 20 and 31 in the block, reset block position and increment by 1

        for(j=0; j<13; j++)
        {	
        	for(i=0; i<24; i++)
        	{
        	    for(r=0; r<6; r++)
        	    {
        	    	if(fread(&left_aud_batch[r], 1, 1, audiof) != 1 || fread(&right_aud_batch[r], 1, 1, audiof) != 1) 
        	    	{
        	        	left_aud_batch[r] = right_aud_batch[r] = 0x80;
        	        	done = true;
        	    	}
        	    }
        	    semi_hq_encode(false, left_aud_batch, right_aud_batch, amplitude_table, left_pulses, right_pulses, &left_mv, &right_mv);
        	    for(r=0; r<6; r++)
        	    {
        	    	block[main_block_pos++] = ((left_pulses[r]<<4)&0xf0)|(right_pulses[r]&0x0f);
        			while((main_block_pos&0x1f)<20)
        			{	
        				main_block_pos++;
        			}
        	    }
        	    
        	    block[mv_block_pos++] = ((left_mv<<4)&0xf0)|(right_mv&0x0f);
        	    while((mv_block_pos&0x1f)<20)
        		{
        			mv_block_pos++;
        		}
        		reverse_audio(false, left_pulses, right_pulses, left_mv, right_mv, &debug_pos, debug_audio);
        	}
        	for(r=0; r<7; r++)
        	{	
        		if(fread(&left_aud_ext_batch[r], 1, 1, audiof) != 1 || fread(&right_aud_ext_batch[r], 1, 1, audiof) != 1) 
        		{
        	    	left_aud_ext_batch[r] = right_aud_ext_batch[r] = 0x80;
        	    	done = true;
        		}
        	}
        	semi_hq_encode(true, left_aud_ext_batch, right_aud_ext_batch, amplitude_table, left_ext_pulses, right_ext_pulses, &left_mv, &right_mv);
        	for(r=0; r<7; r++)
        	{
        	    block[main_block_pos++] = ((left_ext_pulses[r]<<4)&0xf0)|(right_ext_pulses[r]&0x0f);
        		while((main_block_pos&0x1f)<20)
        		{	
        			main_block_pos++;
        		}
        	}
        	block[mv_block_pos++] = ((left_mv<<4)&0xf0)|(right_mv&0x0f);
        	while((mv_block_pos&0x1f)<20)
        	{
        		mv_block_pos++;
        	}
        	reverse_audio(true, left_ext_pulses, right_ext_pulses, left_mv, right_mv, &debug_pos, debug_audio);
        	for(i=0; i<25; i++)
        	{
        	    for(r=0; r<6; r++)
        	    {
        	    	if(fread(&left_aud_batch[r], 1, 1, audiof) != 1 || fread(&right_aud_batch[r], 1, 1, audiof) != 1) 
        	    	{
        	        	left_aud_batch[r] = right_aud_batch[r] = 0x80;
        	        	done = true;
        	    	}
        	    }
        	    semi_hq_encode(false, left_aud_batch, right_aud_batch, amplitude_table, left_pulses, right_pulses, &left_mv, &right_mv);
        	    for(r=0; r<6; r++)
        	    {
        	    	block[main_block_pos++] = ((left_pulses[r]<<4)&0xf0)|(right_pulses[r]&0x0f);
        			while((main_block_pos&0x1f)<20)
        			{	
        				main_block_pos++;
        			}
        	    }
        	    block[mv_block_pos++] = ((left_mv<<4)&0xf0)|(right_mv&0x0f);
        	    while((mv_block_pos&0x1f)<20)
        		{
        			mv_block_pos++;
        		}
        		reverse_audio(false, left_pulses, right_pulses, left_mv, right_mv, &debug_pos, debug_audio);
        	}
        	for(r=0; r<7; r++)
        	{	
        		if(fread(&left_aud_ext_batch[r], 1, 1, audiof) != 1 || fread(&right_aud_ext_batch[r], 1, 1, audiof) != 1) 
        		{
        	    	left_aud_ext_batch[r] = right_aud_ext_batch[r] = 0x80;
        	    	done = true;
        		}
        	}
        	semi_hq_encode(true, left_aud_ext_batch, right_aud_ext_batch, amplitude_table, left_ext_pulses, right_ext_pulses, &left_mv, &right_mv);
        	for(r=0; r<7; r++)
        	{
        	    block[main_block_pos++] = ((left_ext_pulses[r]<<4)&0xf0)|(right_ext_pulses[r]&0x0f);
        		while((main_block_pos&0x1f)<20)
        		{	
        			main_block_pos++;
        		}
        	}
        	block[mv_block_pos++] = ((left_mv<<4)&0xf0)|(right_mv&0x0f);
        	while((mv_block_pos&0x1f)<20)
        	{
        		mv_block_pos++;
        	}
        	reverse_audio(true, left_ext_pulses, right_ext_pulses, left_mv, right_mv, &debug_pos, debug_audio);
        	
        	main_block_pos = audio_offset;
        	mv_block_pos = base_mv_offset;

        	memcpy(&output[main_pos], &block[0], block_offset);

        	if((0x4000-(main_pos&0x3fff))>=block_offset) //check to see if the next block is in the same bank
        	{
        		main_pos += block_offset;
        	}
        	//if not, don't increment main_pos so that the bank offset can work properly
        }
        //add base_mv_offset to base_main_offset for mv offset for new block

		//READ THIS! THIS IS VERY IMPORTANT!
		//When incrementing a frame block, go back to base_main_offset, then add base_mv_offset for the mv position, then add block_offset to base_main_offset AFTER copying it to main_pos

        main_pos &= ~0x3fff;
        main_pos += 0x4000;
    }

    done = false;
    main_pos = 0x4000 + TILE_DATA_SIZE;
    fclose(audiof);
    printf("Testing the GBC-2bpp-video audio encoder! \n");
    FILE *out_aud = fopen(argv[3], "wb");
    if(!out_aud) 
    {
        perror("I was expecting this to break anyway lol :cry:");
        return 1;
    }
    fwrite(&output, 1, sizeof(output), out_aud);
    fclose(out_aud);

    FILE *debug_aud = fopen(argv[4], "wb");
    if(!debug_aud)
    {
    	return 1;
    }
    fwrite(&debug_audio, 1, sizeof(debug_audio), debug_aud);
    fclose(debug_aud);

    /*
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
    */
}

