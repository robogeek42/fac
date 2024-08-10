SRC_FILES="keydefines.h colmap.c colmap.h filedialog.h globals.h item.c item.h listdir.c listdir.h progbar.c progbar.h util.c util.h"
for file in $SRC_FILES
do
	ln -s ../../src/${file} src/${file}
done
