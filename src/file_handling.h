/*
  vim:ts=4
  vim:sw=4
*/

// include this file directly - just collects together file handling functions
//
//bool check_dir_exists(char *path);
//int load_map(char *mapname);
//int load_map_info( char *filename);
//void load_resource_data();
//void clear_map_and_lists();
//bool save_game( char *filepath );
//bool load_game( char *filepath, bool bQuiet );
bool load_game_map( char * game_map_name );

bool alloc_map()
{
	tilemap = (uint8_t *) malloc(sizeof(uint8_t) * mapinfo.width * mapinfo.height);
	if (tilemap == NULL)
	{
		printf("Out of memory\n");
		return false;
	}

	layer_belts = (int8_t *) malloc(sizeof(int8_t) * mapinfo.width * mapinfo.height);
	if (layer_belts == NULL)
	{
		printf("Out of memory\n");
		return false;
	}
	memset(layer_belts, (int8_t)-1, mapinfo.width * mapinfo.height);

	objectmap = (void**) malloc(sizeof(void*) * mapinfo.width * mapinfo.height);
	if (objectmap == NULL)
	{
		printf("Out of memory\n");
		return false;
	}
	memset(objectmap, 0, 3*mapinfo.width * mapinfo.height);

	return true;
}

bool check_dir_exists(char *path)
{
    FRESULT        fr = FR_OK;
    DIR            dir;
    fr = ffs_dopen(&dir, path);
	if ( fr == FR_OK )
	{
		return true;
	}
	return false;
}
int load_map_info( char *filename)
{
	uint8_t ret = mos_load( filename, (uint24_t) &mapinfo,  2 );
	if ( ret != 0 )
	{
		printf("Failed to load %s\n",filename);
	}
	return ret;
}

void load_resource_data()
{
	for (int y=0; y<mapinfo.height; y++)
	{
		for (int x=0; x<mapinfo.width; x++)
		{
			int offset = x + mapinfo.width*y;
			int overlay = getOverlayAtOffset( offset );
			if (overlay>0)
			{
				int val = fac.resourceMultiplier * 3;
				if (overlay > 5) val -= fac.resourceMultiplier;
				if (overlay > 10) val -= fac.resourceMultiplier;
				insertAtFrontItemList( &resourcelist, val, x, y);
			}
		}
	}
}
int load_map(char *mapname)
{
	uint8_t ret = mos_load( mapname, (uint24_t) tilemap,  mapinfo.width * mapinfo.height );
	if ( ret != 0 )
	{
		return ret;
	}
	
	load_resource_data();
	return ret;
}

void clear_map_and_lists()
{
	// clear and read tilemap and layers
	free(tilemap);
	free(layer_belts);
	free(objectmap);

	// clear out item list
	ItemNodePtr currPtr = itemlist;
	ItemNodePtr nextPtr = NULL;
	while ( currPtr != NULL )
	{
		nextPtr = currPtr->next;
		ItemNodePtr pitem = popFrontItem(&itemlist);
		free(pitem);
		currPtr = nextPtr;
	}
	itemlist = NULL;

	clearMachines(&machinelist);
	clearInserters(&inserterlist);
	
	// clear out resources list
	currPtr = resourcelist;
	nextPtr = NULL;
	while ( currPtr != NULL )
	{
		nextPtr = currPtr->next;
		ItemNodePtr pitem = popFrontItem(&resourcelist);
		free(pitem); pitem=NULL;
		currPtr = nextPtr;
	}
	resourcelist = NULL;
}

bool save_game( char *filepath )
{
	bool ret = true;
	FILE *fp;
	int objs_written = 0;
	char *msg;

	COL(15);

	// Open the file for writing
	if ( !(fp = fopen( filepath, "wb" ) ) ) {
		printf( "Error opening file \"%s\"a\n.", filepath );
		return false;
	}

	// 0. write file signature to 1st 3 bytes "FAC"	
	printf("Save: signature\n");
	for (int i=0;i<3;i++)
	{
		fputc( sigref[i], fp );
	}

	// 1. write the fac state
	printf("Save: fac state\n");
	objs_written = fwrite( (const void*) &fac, sizeof(FacState), 1, fp);
	if (objs_written!=1) {
		msg = "Fail: fac state\n"; goto save_game_errexit;
	}

	// 2. write the tile map and layers
	printf("Save: tilemap\n");
	objs_written = fwrite( (const void*) tilemap, sizeof(uint8_t) * mapinfo.width * mapinfo.height, 1, fp);
	if (objs_written!=1) {
		msg = "Fail: tilemap\n"; goto save_game_errexit;
	}
	printf("Save: layer_belts\n");
	objs_written = fwrite( (const void*) layer_belts, sizeof(uint8_t) * mapinfo.width * mapinfo.height, 1, fp);
	if (objs_written!=1) {
		msg = "Fail: layer_belts\n"; goto save_game_errexit;
	}

	// 3. save the item list

	// get and write number of items
	ItemNodePtr currPtr = itemlist;
	int cnt=0;
	while (currPtr != NULL) {
		currPtr = currPtr->next;
		cnt++;
	}
	printf("Save: num item count %d\n",cnt);
	objs_written = fwrite( (const void*) &cnt, sizeof(int), 1, fp);
	if (objs_written!=1) {
		msg = "Fail: item count\n"; goto save_game_errexit;
	}

	// back to begining
	currPtr = itemlist;
	ItemNodePtr nextPtr = NULL;

	// must write data and no ll pointers
	printf("Save: items\n");
	while (currPtr != NULL) {
		nextPtr = currPtr->next;

		objs_written = fwrite( (const void*) currPtr, sizeof(ItemNodeSave), 1, fp);
		if (objs_written!=1) {
			msg = "Fail: item\n"; goto save_game_errexit;
		}
		currPtr = nextPtr;
	}

	// 4. write the inventory
	printf("Save: inventory\n");
	objs_written = fwrite( (const void*) inventory, sizeof(INV_ITEM), MAX_INVENTORY_ITEMS, fp);
	if (objs_written!=MAX_INVENTORY_ITEMS) {
		msg = "Fail: inventory\n"; goto save_game_errexit;
	}

	// 5. write machine data
	printf("Save: machines\n");
	int num_machine_objects = countMachine(&machinelist);
	printf("%d objects\n",num_machine_objects);
	objs_written = fwrite( (const void*) &num_machine_objects, sizeof(int), 1, fp);
	if (objs_written!=1) {
		msg = "Fail: machine count\n"; goto save_game_errexit;
	}

	ThingNodePtr thptr = machinelist;
	while (thptr != NULL )
	{
		// save the machine without the items
		Machine *mach = (Machine*)thptr->thing;

		objs_written = fwrite( (const void*) mach, sizeof(MachineSave), 1, fp);
		if (objs_written!=1) {
			msg = "Fail: machines\n"; goto save_game_errexit;
		}

		thptr = thptr->next;
	}

	// 6. write the resource data
	currPtr = resourcelist;
	cnt=0;
	while (currPtr != NULL) {
		currPtr = currPtr->next;
		cnt++;
	}
	printf("Save: num resources count %d\n",cnt);
	objs_written = fwrite( (const void*) &cnt, sizeof(int), 1, fp);
	if (objs_written!=1) {
		msg = "Fail: resource count\n"; goto save_game_errexit;
	}

	// back to begining
	currPtr = resourcelist;
	nextPtr = NULL;

	// must write data and no ll pointers
	printf("Save: resources\n");
	while (currPtr != NULL) {
		nextPtr = currPtr->next;

		objs_written = fwrite( (const void*) currPtr, sizeof(ItemNodeSave), 1, fp);
		if (objs_written!=1) {
			msg = "Fail: item\n"; goto save_game_errexit;
		}
		currPtr = nextPtr;
	}

	// 7. write the Inserter objects
	printf("Save: inserters ");
	int num_inserter_objects = countInserters(&inserterlist);
	printf("%d objects\n",num_inserter_objects);
	objs_written = fwrite( (const void*) &num_inserter_objects, sizeof(int), 1, fp);
	if (objs_written!=1) {
		msg = "Fail: inserter count\n"; goto save_game_errexit;
	}

	thptr = inserterlist;
	Inserter *insp = NULL;
	while (thptr != NULL )
	{
		// save the inserter without the items
		insp = (Inserter*)thptr->thing;

		objs_written = fwrite( (const void*) insp, sizeof(InserterSave), 1, fp);
		if (objs_written!=1) {
			msg = "Fail: inserter\n"; goto save_game_errexit;
		}

		thptr = thptr->next;
	}

	printf("done.\n");
	fclose(fp);
	return ret;

save_game_errexit:
	COL(9);printf("%s",msg);
	fclose(fp);
	COL(15);
	return false;

}
bool load_game( char *filepath, bool bQuiet )
{
	// 0. write file signature to 1st 3 bytes "FAC"	
	// 1. write the fac state
	// 2. write the tile map and layers
	// 3. save the item list
	// 4. write the inventory
	// 5. write machine data
	// 6. write the resource data
	bool ret = true;
	FILE *fp;
	int objs_read = 0;
	char *msg;

	COL(15);

	// open file for reading
	if ( !(fp = fopen( filepath, "rb" ) ) ) {
		printf("Error opening file \"%s\".\n", filepath );
		return false;
	}

	char sig[4];

	// 0. read file signature to 1st 3 bytes "FAC"	
	if (!bQuiet) printf("Load: signature\n");
	fgets( sig, 4, fp );

	for (int i=0; i<3; i++)
	{
		if ( sig[i] != sigref[i] )
		{
			printf("Error reading signature. i=%d\n", i );
			fclose(fp);
			return false;
		}
	}

	// 1. read the fac state
	if (!bQuiet) printf("Load: fac state\n");
	objs_read = fread( &fac, sizeof(FacState), 1, fp );
	if ( objs_read != 1 )
	{
		msg = "Fail reading state\n"; goto load_game_errexit;
	}
	if ( fac.version != SAVE_VERSION )
	{
		printf("Fail save has v%d require v%d\n", fac.version, SAVE_VERSION );
		/* reset bob and screen centred in map */
		fac.bobx = (mapinfo.width * gTileSize / 2) & 0xFFFFF0;
		fac.boby = (mapinfo.height * gTileSize / 2) & 0xFFFFF0;
		fac.xpos = fac.bobx - gScreenWidth/2;
		fac.ypos = fac.boby - gScreenHeight/2;

		msg = ""; goto load_game_errexit;
	}

	// default resource multiplier
	if ( fac.resourceMultiplier == 0 )
	{
		fac.resourceMultiplier = 16;
	}

	// 1.5 Load the map using the mapname stored in the fac state
	if (!bQuiet) printf("Load: map %s\n",fac.mapname);

	// clear and read tilemap and layers
	clear_map_and_lists();
	if ( !load_game_map(fac.mapname) )
	{
		msg="Failed to load map"; goto load_game_errexit;
	}
	printf("MAP: %dx%d\n",mapinfo.width, mapinfo.height);

	// 2. read the tile map and layers

	// read the tilemap
	if (!bQuiet) printf("Load: tilemap\n");
	objs_read = fread( tilemap, sizeof(uint8_t) * mapinfo.width * mapinfo.height, 1, fp );
	if ( objs_read != 1 ) {
		msg = "Fail read tilemap\n"; goto load_game_errexit;
	}
	// read the layer_belts
	if (!bQuiet) printf("Load: layer_belts\n");
	objs_read = fread( layer_belts, sizeof(uint8_t) * mapinfo.width * mapinfo.height, 1, fp );
	if ( objs_read != 1 ) {
		msg = "Fail read layer_belts\n"; goto load_game_errexit;
	}
	// objectmap is a list of pointers so will be filled in when we read the machines/inserters

	// 3. read the item list
	
	// read number of items in list
	int num_items = 0;

	if (!bQuiet) printf("Load: items ");
	objs_read = fread( &num_items, sizeof(int), 1, fp );
	if ( objs_read != 1 ) {
		msg = "Fail read num items\n"; goto load_game_errexit;
	}
	printf("%d\n",num_items);

	// add items in one by one
	while (num_items > 0 && !feof( fp ) )
	{
		ItemNodeSave newitem;

		objs_read = fread( &newitem, sizeof(ItemNodeSave), 1, fp );
		if ( objs_read != 1 ) {
			msg = "Fail read item\n"; goto load_game_errexit;
		}
		insertAtBackItemList( &itemlist, newitem.item, newitem.x, newitem.y );
		num_items--;
	}

	// 4. write the inventory
	if (!bQuiet) printf("Load: inventory\n");
	for ( int i=0;i<MAX_INVENTORY_ITEMS; i++)
	{
		objs_read = fread( &inventory[i], sizeof(INV_ITEM), 1, fp );
		if ( objs_read != 1 ) {
			msg = "Fail read inv item\n"; goto load_game_errexit;
		}
	}

	// 5. read machine data
	int num_machs = 0;

	if (!bQuiet) printf("num ");
	objs_read = fread( &num_machs, sizeof(int), 1, fp );
	if ( objs_read != 1 ) {
		msg = "Fail read num machines\n"; goto load_game_errexit;
	}
	if (!bQuiet) printf("%d ",num_machs);
	while (num_machs > 0 && !feof( fp ) )
	{
		Machine* newmachp = malloc(sizeof(Machine));
		if (newmachp == NULL)
		{
			msg="Alloc Error\n"; goto load_game_errexit;
		}

		objs_read = fread( newmachp, sizeof(MachineSave), 1, fp );
		if ( objs_read != 1 ) {
			msg = "Fail read machines\n"; goto load_game_errexit;
		}
		// start machines with an empty item list
		newmachp->itemlist = NULL;
		newmachp->ticksTillProduce = 0;
		// make sure splitters don't have ghosts of items in them
		if ( newmachp->machine_type == IT_TSPLITTER ) newmachp->processTime = 0;

		insertAtBackThingList( &machinelist, newmachp);
		objectmap[newmachp->tx + newmachp->ty * mapinfo.width] = newmachp;
		num_machs--;
	}
	if (!bQuiet) printf("done.\n");

	// 6. read the resource data
	
	resourcelist = NULL;

	// read number of resources in list
	num_items = 0;

	if (!bQuiet) printf("num_items ");
	objs_read = fread( &num_items, sizeof(int), 1, fp );
	if ( objs_read != 1 ) {
		msg = "Fail read num resources\n"; goto load_game_errexit;
	}
	if (!bQuiet) printf("%d ",num_items);

	// add items in one by one
	while (num_items > 0 && !feof( fp ) )
	{
		ItemNodeSave newitem;

		objs_read = fread( &newitem, sizeof(ItemNodeSave), 1, fp );
		if ( objs_read != 1 ) {
			msg = "Fail read resource\n"; goto load_game_errexit;
		}
		insertAtBackItemList( &resourcelist, newitem.item, newitem.x, newitem.y );
		num_items--;
	}
	if (!bQuiet) printf("done.\n");

	// 7. read the Inserter objects

	int num_ins = 0;

	if (!bQuiet) printf("num ");
	objs_read = fread( &num_ins, sizeof(int), 1, fp );
	if ( objs_read != 1 ) {
		msg = "Fail read num inserters\n"; goto load_game_errexit;
	}
	if (!bQuiet) printf("%d ",num_ins);
	while (num_ins > 0 && !feof( fp ) )
	{
		Inserter* newinsp = malloc(sizeof(Inserter));
		if (newinsp == NULL)
		{
			msg="Alloc Error\n"; goto load_game_errexit;
		}

		objs_read = fread( newinsp, sizeof(InserterSave), 1, fp );
		if ( objs_read != 1 ) {
			msg = "Fail read inserter\n"; goto load_game_errexit;
		}
		newinsp->itemlist = NULL;
		newinsp->itemcnt = 0;

		insertAtBackThingList( &inserterlist, newinsp);
		objectmap[newinsp->tx + newinsp->ty * mapinfo.width] = newinsp;
		num_ins--;
	}
	if (!bQuiet) printf("done.\n");

	if (!bQuiet) printf("\nDone.\n");
	fclose(fp);
	return ret;

load_game_errexit:
	COL(9);printf("%s",msg);
	fclose(fp);
	COL(15);
	return false;
}


bool load_game_map( char * game_map_name )
{
	char buf[MAX_MAPNAME_SIZE + 6];
	snprintf(buf, MAX_MAPNAME_SIZE + 6, "%s.info",game_map_name);
	if ( load_map_info(buf) != 0 )
	{
		printf("Failed to load map info");
		return false;
	}

	if ( !alloc_map() ) 
	{
		printf("Failed to alloc map layers\n");
		return false;
	}

	if (load_map(game_map_name) != 0)
	{
		printf("Failed to load map\n");
		return false;
	}

	strncpy(fac.mapname, game_map_name, MAX_MAPNAME_SIZE); 

	return true;
}


// returns length of sound sample
int load_sound_sample(char *fname, int sample_id)
{
	unsigned int file_len = 0;
	FILE *fp;
	fp = fopen(fname, "rb");
	if ( !fp )
	{
		printf("Fail to Open %s\n",fname);
		return 0;
	}
	// get length of file
	fseek(fp, 0, SEEK_END);
	file_len = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	uint8_t *data = (uint8_t*) malloc(file_len);
	if ( !data )
	{
		fclose(fp);
		printf("Mem alloc error\n");
		return 0;
	}
	unsigned int bytes_read = fread( data, 1, file_len, fp );
	if ( bytes_read != file_len )
	{
		fclose(fp);
		printf("Err read %d bytes expected %d\n", bytes_read, file_len);
		return 0;
	}

	fclose(fp);

	if (file_len)
	{
		vdp_audio_load_sample(sample_id, file_len, data);
	}

	return file_len;
}

