.PHONY: avio_dir_cmd avio_reading decode_audio decode_video \
			demuxing_decoding encode_audio encode_video

avio_dir_cmd:
	gcc /src/avio_dir_cmd.c -o ./bin/avio_dir_cmd -g `pkg-config \
		--libs --cflags libavformat libavcodec libavutil`
	cp ./bin/avio_dir_cmd ./run

avio_reading:
	gcc ./src/avio_reading.c -o ./bin/avio_reading -g `pkg-config \
		--libs --cflags libavcodec libavformat libavutil`
	cp ./bin/avio_reading ./run

decode_audio:
	gcc ./src/decode_audio.c -o ./bin/decode_audio -g `pkg-config \
		--libs --cflags libavutil libavcodec`
	cp ./bin/decode_audio ./run

decode_video:
	gcc ./src/decode_video.c -o ./bin/decode_video -g `pkg-config \
		--libs --cflags libavutil libavcodec`
	cp ./bin/decode_video ./run

demuxing_decoding:
	gcc /src/demuxing_decoding.c -o ./bin/demuxing_decoding -g `pkg-config \
		--libs --cflags libavutil libavcodec libavformat`
	cp ./bin/demuxing_decoding ./run

encode_audio:
	gcc ./src/encode_audio.c -o ./bin/encode_audio -g `pkg-config \
		--libs --cflags libavutil libavcodec` -lm
	cp ./bin/encode_audio ./run

encode_video:
	gcc ./src/encode_video.c -o ./bin/encode_video -g `pkg-config \
		--libs --cflags libavutil libavcodec` -lm
	cp ./bin/encode_video ./run

