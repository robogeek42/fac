#!/bin/bash 
if [ $# -lt 1 ]
then
	echo "usage: $0 <install directory>"
	echo "e.g. ./install.sh $HOME/sdcard/games/fac"
	exit -1
fi
FACDIR=$1

echo "Installing to $FACDIR"
if [ ! -d $FACDIR ]
then
	mkdir -p $FACDIR
fi

echo "copy assets"
rsync -rvu --progress --delete img/ $FACDIR/img
rsync -rvu --progress --delete sounds/ $FACDIR/sounds

echo "copy default maps"
if [ ! -d $FACDIR/maps ]
then
	mkdir -p $FACDIR/maps
fi
cp maps/splash2* $FACDIR/maps
cp maps/splash_save.data $FACDIR/maps
cp maps/m4mod* $FACDIR/maps
cp maps/m5res* $FACDIR/maps
cp maps/newmap1* $FACDIR/maps

echo "create saves dir"
if [ ! -d $FACDIR/saves ]
then
	mkdir -p $FACDIR/saves
fi

echo "copy binaries"
cp fac.bin facnl.bin fed.bin fednl.bin loader.bin preload.bin $FACDIR/
cp go.obey $FACDIR/

cat << EOF
done.

TO RUN FAC (on newer MOS): 

cd $FACDIR 
fac

or, for the editor, run
fed


EOF
