# Pulled from GBVideoPlayer2 for initial testing because I'm even noobier at makefile than C
# Really, the only purpose this serves is making sure the input audio is amplified enough
# and also to make the debug audio file. (Don't have anything to convert it from unsigned
# 8-bit PCM to WAV or MP3 so... yeah.)

ifneq ($(MAKECMDGOALS),clean)
ifeq ($(SOURCE),)
$(error Missing video source. Use "make SOURCE=...")
endif
endif

OUT := output/$(basename $(notdir $(SOURCE)))
$(shell mkdir -p $(OUT))
CC ?= clang
FFMPEG := ffmpeg -loglevel warning -stats -hide_banner

TITLE = "\033[1m\033[36m"
TITLE_END = "\033[0m"

$(OUT)/video.bin: output/encoder $(OUT)/audio.raw
	@echo $(TITLE)Encoding audio...$(TITLE_END)
	output/encoder $(shell ffmpeg -i $(SOURCE) 2>&1 | sed -n "s/.*, \(.*\) fp.*/\1/p") \
	               $(OUT)/audio.raw \
	               $@ \
	               $(OUT)/debug.raw \

output/encoder: encoder.c
	@echo $(TITLE)Compiling encoder...$(TITLE_END)
	$(CC) -g -Ofast -std=c11 -Wall -o $@ $^

$(OUT)/audio.raw: $(SOURCE)
	@echo $(TITLE)Converting audio...$(TITLE_END)
	$(eval GAIN := 0$(shell ffmpeg -i $^ -filter:a volumedetect -f null /dev/null 2>&1 | sed -n "s/.*max_volume: -\(.*\) dB/\1/p"))
	$(FFMPEG) -i $^ -f u8 -acodec pcm_u8 -ar 9198 -filter:a "volume=$(GAIN)dB" $@ 
	
clean:
	rm -rf output