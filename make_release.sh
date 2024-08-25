#!/bin/bash 
if [ $# -lt 1 ]
then
	echo "usage: $0 <release version>"
	echo "e.g. ./make_release.sh "
	exit -1
fi
FACDIR=fac_${1}

echo "Installing to $FACDIR"
if [ ! -d $FACDIR ]
then
	mkdir -p $FACDIR
fi

echo "copy assets"
rsync -rvu --progress img/ $FACDIR/img
rsync -rvu --progress sounds/ $FACDIR/sounds

echo "copy default maps"
if [ ! -d $FACDIR/maps ]
then
	mkdir -p $FACDIR/maps
fi
cp maps/splash2* $FACDIR/maps
cp maps/splash_save.data $FACDIR/maps
cp maps/m4mod* $FACDIR/maps
cp maps/m5res* $FACDIR/maps

echo "create saves dir"
if [ ! -d $FACDIR/saves ]
then
	mkdir -p $FACDIR/saves
fi

echo "copy binary"
cp bin/fac.bin $FACDIR

echo "done"

zip -r ${FACDIR}.zip $FACDIR

rm -rf $FACDIR
