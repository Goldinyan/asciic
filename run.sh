#!/usr/bin/env bash

CONFIG="$1"
YT_URL="$2"

if [ -z "$CONFIG" ]; then
    echo "Usage: $0 <WIDTHxHEIGHT:FPS> [YOUTUBE_URL]"
    echo "Example: $0 300x200:30 https://www.youtube.com/watch?v=dQw4w9WgXcQ"
    exit 1
fi

# split strin using x and : as delimiters
IFS='x:' read -r width height fps <<< "$CONFIG"

echo "Width:  $width"
echo "Height: $height"
echo "FPS:    $fps"

gcc main.c -o main -lm

# check if yt if not local file
if [ -n "$YT_URL" ]; then
    echo "Fetching stream URL using yt-dlp..."
    # yt-dlp -g gets the direct media stream link so ffmpeg can stream it instantly, i hope, it should not break
    INPUT_SOURCE=$(yt-dlp -f "bestvideo" -g "$YT_URL")
else
    INPUT_SOURCE="video.mp4"
fi


echo "[\027[34m INFO \027[0m] Starting..."
ffmpeg -re -loglevel quiet -i "$INPUT_SOURCE" -f rawvideo -pix_fmt rgb24 -s "${width}x${height}" -r "$fps" - | ./main "$CONFIG"
