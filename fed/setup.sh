cd src
for file in colmap.c colmap.h filedialog.h globals.h images.h item.c item.h listdir.c listdir.h progbar.c progbar.h
do
	if [ ! -f $file ]
	then
		ln -s ../../src/$file $file
	fi
done
cd -
