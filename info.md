# Structures and data layout info

## Mappings for Features and Items
3 types of each cores
```
	stone    0:5:10 
	iron ore 1:6:11
	copp ore 2:7:12
	coal     3:8:13
```
Bitmaps for these start at `BMOFF_FEAT16`

**`tilemap`** is these numbers+1
	uint8_t tilemap[] (255=empty) bits 0-3 for tile  

**`uint8_t itemtype[]`** 0-16 currently. All types that can have info/be placed
- item == index
- type : belt/machine/resource/overlay(==feature)
- bmID : to reprsent item (only one for features, stone/ores/wood)
- size : size16 or 8. Used to offset drawing in 16x16 grid


**`itemlist`** Linked list of placed items and location.
	index into itemtype[] for info

**`item_feature_map[]`** maps a bmID to Feature type
- Lookup with overlay/feature number adjusted to 0

**`process_map[]`** maps features to their raw and processed types
- Lookip with item feature number offset by `IT_FEAT_STONE`


	
