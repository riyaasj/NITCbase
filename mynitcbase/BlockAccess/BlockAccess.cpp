#include "BlockAccess.h"
#include <iostream>
#include <cstring>


RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op) {

  //we need to know till where search has taken place
  //get the previous search index of the relation relId from the relation cache
  RecId prevRecId;
  RelCacheTable::getSearchIndex(relId, &prevRecId);

  int block = -1, slot = -1;

  //no hits from previous search, start from the start
  if (prevRecId.block == -1 && prevRecId.slot == -1) {

    //we need the first block of the relation, which is in the relCatEntry
    RelCatEntry RelCatBuf;
    RelCacheTable::getRelCatEntry(relId, &RelCatBuf);

    block = RelCatBuf.firstBlk;
    slot = 0;

  } 

  //there is a hit from previous search
  else {
    block = prevRecId.block;
    slot = prevRecId.slot + 1;
  }

  //searches for the next record in the relation that satisfies the given condition

  //relation remains the same so cache entry the same
  RelCatEntry relCatBuffer;
	RelCacheTable::getRelCatEntry(relId, &relCatBuffer);
  
  while (block != -1) {
    /* create a RecBuffer object for block (use RecBuffer Constructor for
       existing block) */
    RecBuffer Buffer(block);

    //get the record at slot
    Attribute CatRecord[RELCAT_NO_ATTRS];
    Buffer.getRecord(CatRecord, slot);
    
    //get the header of the block
    HeadInfo header;
    Buffer.getHeader(&header);

    //get slot map of the block(we need to check if a slot is free)
    unsigned char *slotMap =(unsigned char *)malloc(sizeof(unsigned char) * header.numSlots);
    Buffer.getSlotMap(slotMap);

    //no more slots in the block
    if (slot >= relCatBuffer.numSlotsPerBlk){
			block = header.rblock;
      slot = 0;
			continue;
		}

    //if slot is free skip the loop
    if (slotMap[slot] == SLOT_UNOCCUPIED) {
      slot++;
      continue;
    }

    //we need the offset of the attribute to compare, this is in the attrCatEntry
    AttrCatEntry attrCatBuf;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatBuf);
    int attrOffset = attrCatBuf.offset;

    // compare record's attribute value to the the given attrVal
    Attribute *record =(Attribute *)malloc(sizeof(Attribute) * header.numAttrs);
    Buffer.getRecord(record, slot);

    int cmpVal = compareAttrs(record[attrOffset], attrVal, attrCatBuf.attrType); 
    if ((op == NE && cmpVal != 0) || // if op is "not equal to"
        (op == LT && cmpVal < 0) ||  // if op is "less than"
        (op == LE && cmpVal <= 0) || // if op is "less than or equal to"
        (op == EQ && cmpVal == 0) || // if op is "equal to"
        (op == GT && cmpVal > 0) ||  // if op is "greater than"
        (op == GE && cmpVal >= 0)    // if op is "greater than or equal to"
    ) {
      
      //set search index
      RecId newIndex;
      newIndex.block = block;
      newIndex.slot = slot;
      RelCacheTable::setSearchIndex(relId, &newIndex);
      return RecId{block, slot};
    }
    slot++;
  }

  // no record in the relation with Id relid satisfies the given condition
  return RecId{-1, -1};
}