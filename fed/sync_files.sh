SRC_FILES="keydefines.h colmap.c colmap.h filedialog.h globals.h images.h item.c item.h listdir.c listdir.h progbar.c progbar.h util.c util.h"
for file in $SRC_FILES
do
	if [ ! -f src/$file ] 
	then
		cp ../src/$file src/
	else 
		d=`diff -q ../src/$file src/$file`
		succ=$?
		if [ $succ -eq 1 ]
		then
			cp ../src/$file src/$file
		fi
	fi
done
