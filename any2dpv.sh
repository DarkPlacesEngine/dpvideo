#!/bin/sh

if [ "$#" -lt 3 ]; then
	echo "Usage: $0 videofile.avi compressionfactor oggquality"
	exit 1
fi

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
shift
shift
shift

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
mplayer -benchmark -vf scale -vo png:z=1 -nosound "$video" "$@"
for X in *.png; do
	pngtopnm "$X" | ppmtotga -rgb > `printf "video_%08d.tga" $((1${X%.png} - 100000001))`
	rm -f "$X"
done
mplayer -vc dummy -vo null -ao pcm "$video" "$@"
dpvencoder video "$ID_VIDEO_FPS" "$compression"
oggenc -q "$oggquality" -o "${video%.*}.ogg" "audiodump.wav"
mv video.dpv "${video%.*}.dpv"
cd "$prevdir"
rm -rf "$dir"
ls -la "$video" "${video%.*}.dpv" "${video%.*}.ogg"
