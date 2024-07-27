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
cp -rf img $FACDIR
cp -rf sounds $FACDIR

echo "copy default maps"
if [ ! -d $FACDIR/maps ]
then
	mkdir -p $FACDIR/maps
fi
cp maps/splash* $FACDIR/maps
cp maps/m4* $FACDIR/maps

echo "create saves dir"
if [ ! -d $FACDIR/saves ]
then
	mkdir -p $FACDIR/saves
fi

echo "copy binary"
cp bin/fac.bin $FACDIR/fac

cat << EOF
done.

TO RUN FAC : 

cd $FACDIR 
load fac
run
EOF
