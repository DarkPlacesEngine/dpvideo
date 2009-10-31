#!/bin/sh

case "$#" in
	3)
		;;
	*)
		echo "Usage: $0 videofile.avi compressionfactor oggquality"
		exit 1
		;;
esac

set -ex

case "$1" in
	/*)
		video=$1
		;;
	*)
		video="$PWD/$1"
		;;
esac
compression=$2
oggquality=$3

midentify()
{
	# This is a wrapper around the -identify functionality.
	# It is supposed to escape the output properly, so it can be easily
	# used in shellscripts by 'eval'ing the output of this script.
	#
	# Written by Tobias Diedrich <ranma+mplayer@tdiedrich.de>
	# Licensed under GNU GPL.
	mplayer -vo null -ao null -frames 0 -identify "$@" 2>/dev/null |
			sed -ne '/^ID_/ {
							  s/[]()|&;<>`'"'"'\\!$" []/\\&/g;p
							}'
}

prevdir=`pwd`
dir=`mktemp -d -t dpvencode.XXXXXX`
cd "$dir"
eval "`midentify "$video"`"
mplayer -benchmark -vf scale -vo tga -nosound "$video"
# make 0-based indexes from 1-based indexes
prev=
for X in *.tga; do
	mv "$X" `printf "video_%08d.tga" $((1${X%.tga} - 100000001))`
	prev=$X
done
mplayer -vc dummy -vo null -ao pcm "$video"
dpvencoder video "$ID_VIDEO_FPS" "$compression"
oggenc -q "$oggquality" -o "${video%.*}.ogg" "audiodump.wav"
mv video.dpv "${video%.*}.dpv"
cd "$prevdir"
rm -rf "$dir"
ls -la "$video" "${video%.*}.dpv" "${video%.*}.ogg"
