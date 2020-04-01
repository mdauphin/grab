grab: main.c
	gcc main.c -o grab -lavcodec -lavcodec -lavformat -lswresample -lavutil -lswscale
depend:
	sudo apt install libavcodec-dev libswresample-dev libavformat-dev libavutil-dev libswscale-dev
