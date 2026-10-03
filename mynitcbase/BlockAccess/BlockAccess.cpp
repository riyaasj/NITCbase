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



int BlockAccess::renameRelation(char oldName[ATTR_SIZE], char newName[ATTR_SIZE]){
    
  //reset search index of realtion catalog
  RelCacheTable::resetSearchIndex(RELCAT_RELID);


  Attribute newRelationName;    
  strcpy(newRelationName.sVal, newName);


  //relation with new relation name already exists
  RecId nls = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, newRelationName, EQ);
  if(nls.block != -1 && nls.slot != -1) return E_RELEXIST;
  
  //reset search index of relation catalog
  RelCacheTable::resetSearchIndex(RELCAT_RELID);
  

  Attribute oldRelationName;   
  strcpy(oldRelationName.sVal, oldName);


  //relation does not exist
  RecId ols = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, oldRelationName, EQ);
  if(ols.block == -1 && ols.slot == -1) return E_RELNOTEXIST;


  RecBuffer Buffer(ols.block);

  //get the relation catalog entry of the relation
  Attribute CatRecord[RELCAT_NO_ATTRS];
  Buffer.getRecord(CatRecord, ols.slot);
  

  //change the name in the record and set
  strcpy(CatRecord[RELCAT_REL_NAME_INDEX].sVal, newName); 
  Buffer.setRecord(CatRecord, ols.slot);


  /*
  update all the attribute catalog entries in the attribute catalog corresponding
  to the relation with relation name oldName to the relation name newName
  */

  //reset search index of relation catalog
  RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

  for(int i = 0; i < CatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal; i++){
    RecId attrCatSearch = BlockAccess::linearSearch(ATTRCAT_RELID, ATTRCAT_ATTR_RELNAME, oldRelationName, EQ);
    RecBuffer attrCatBlock(attrCatSearch.block);
    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
    attrCatBlock.getRecord(attrCatRecord, attrCatSearch.slot);
    strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, newName);
    attrCatBlock.setRecord(attrCatRecord, attrCatSearch.slot);
  }


  return SUCCESS;
}



int BlockAccess::renameAttribute(char relName[ATTR_SIZE], char oldName[ATTR_SIZE], char newName[ATTR_SIZE]) {

  //reset search index of relation catalog
  RelCacheTable::resetSearchIndex(RELCAT_RELID);

  Attribute relNameAttr;
  strcpy(relNameAttr.sVal, relName);


  RecId relcatRecId = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, relNameAttr, EQ);

  //relationa does not exist
  if (relcatRecId.block == -1 and relcatRecId.slot == -1) return E_RELNOTEXIST;

  //reset search index of attribute catalog
  RelCacheTable::resetSearchIndex(ATTRCAT_RELID);


  RecId attrToRenameRecId{-1, -1};
  Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];

  /* iterate over all Attribute Catalog Entry record corresponding to the
     relation to find the required attribute */
  while(true){
    
    //find the attribute catalog entries with relation name relName
    RecId searchIndex = BlockAccess::linearSearch(ATTRCAT_RELID, ATTRCAT_ATTR_RELNAME, relNameAttr, EQ);
    
    //no more attributes to check
    if (searchIndex.block == -1 and searchIndex.slot == -1) break;


    //get the attrubute catalog entry
    RecBuffer attrCatBlock(searchIndex.block);
    attrCatBlock.getRecord(attrCatEntryRecord, searchIndex.slot);


    //if you found the attribute with oldName yay!
    if (strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, oldName) == 0) {
      attrToRenameRecId = searchIndex;
      break;
    }


    //if you found an attribute already with newName boo!
    if (strcmp(attrCatEntryRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newName) == 0){
			return E_ATTREXIST;
		}
  }

  //attribute with oldName does not exist
  if(attrToRenameRecId.slot == -1 and attrToRenameRecId.block == -1){
    return E_ATTRNOTEXIST;
  }

  //why couldnt you just have done this in the loop? because we needed to check if the newName was already there
  RecBuffer attrCatBlock(attrToRenameRecId.block);
  Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
  attrCatBlock.getRecord(attrCatRecord,attrToRenameRecId.slot);
  strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal,newName);
  attrCatBlock.setRecord(attrCatRecord,attrToRenameRecId.slot);
  return SUCCESS;
}



int BlockAccess::insert(int relId, Attribute *record) {
  RelCatEntry relCatEntry;
  RelCacheTable::getRelCatEntry(relId, &relCatEntry);
  // get the relation catalog entry from relation cache
  // ( use RelCacheTable::getRelCatEntry() of Cache Layer)

  int blockNum = relCatEntry.firstBlk; /* first record block of the relation
                                          (from the rel-cat entry)*/
  ;

  // rec_id will be used to store where the new record will be inserted
  RecId rec_id = {-1, -1};

  int numOfSlots =
      relCatEntry.numSlotsPerBlk; /* number of slots per record block */
  ;
  int numOfAttributes =
      relCatEntry.numAttrs; /* number of attributes of the relation */
  ;

  int prevBlockNum =
      -1; /* block number of the last element in the linked list = -1 */
  ;

  /*
      Traversing the linked list of existing record blocks of the relation
      until a free slot is found OR
      until the end of the list is reached
  */

  while (blockNum != -1) {
    RecBuffer recBuffer(blockNum);
    HeadInfo header;
    recBuffer.getHeader(&header);
    // create a RecBuffer object for blockNum (using appropriate constructor!)

    // get header of block(blockNum) using RecBuffer::getHeader() function
    unsigned char *slotMap =
        (unsigned char *)malloc(sizeof(unsigned char) * header.numSlots);
    recBuffer.getSlotMap(slotMap);
    // get slot map of block(blockNum) using RecBuffer::getSlotMap() function

    // search for free slot in the block 'blockNum' and store it's rec-id in
    // rec_id (Free slot can be found by iterating over the slot map of the
    // block)
    /* slot map stores SLOT_UNOCCUPIED if slot is free and
       SLOT_OCCUPIED if slot is occupied) */
    for (int slot = 0; slot < header.numSlots; slot++) {
      if (slotMap[slot] == SLOT_UNOCCUPIED) {
        rec_id.block = blockNum;
        rec_id.slot = slot;
        break;
      }
    }
    if (rec_id.slot != -1 and rec_id.block != -1) {
      break;
    }

    prevBlockNum = blockNum;
    blockNum = header.rblock;

    /* if a free slot is found, set rec_id and discontinue the traversal
       of the linked list of record blocks (break from the loop) */

    /* otherwise, continue to check the next block by updating the
       block numbers as follows:
          update prevBlockNum = blockNum
          update blockNum = header.rblock (next element in the linked
                                           list of record blocks)
    */
  }

  //  if no free slot is found in existing record blocks (rec_id = {-1, -1})
  if (rec_id.block == -1 and rec_id.slot == -1) {
    // if relation is RELCAT, do not allocate any more blocks
    //     return E_MAXRELATIONS;
    if (relId == RELCAT_RELID) {
      return E_MAXRELATIONS;
    }

    // Otherwise,
    // get a new record block (using the appropriate RecBuffer constructor!)
    // get the block number of the newly allocated block
    // (use BlockBuffer::getBlockNum() function)
    RecBuffer blockBuffer;
    blockNum = blockBuffer.getBlockNum();
    // let ret be the return value of getBlockNum() function call
    if (blockNum == E_DISKFULL) {
      return E_DISKFULL;
    }

    // Assign rec_id.block = new block number(i.e. ret) and rec_id.slot = 0
    rec_id.block = blockNum;
    rec_id.slot = 0;

    /*
        set the header of the new record block such that it links with
        existing record blocks of the relation
        set the block's header as follows:
        blockType: REC, pblock: -1
        lblock
              = -1 (if linked list of existing record blocks was empty
                     i.e this is the first insertion into the relation)
              = prevBlockNum (otherwise),
        rblock: -1, numEntries: 0,
        numSlots: numOfSlots, numAttrs: numOfAttributes
        (use BlockBuffer::setHeader() function)
    */
    HeadInfo blockheader;
    blockheader.pblock = blockheader.rblock = -1;
    blockheader.blockType = REC;
    blockheader.lblock = -1;
    // if (relCatEntry.numRecs == 0) {
    //    blockheader.lblock = -1;
    // } else {
    //   blockheader.lblock = prevBlockNum;
    // }
    blockheader.lblock=-1;
    blockheader.numAttrs = relCatEntry.numAttrs;
    blockheader.numEntries = 0;
    blockheader.numSlots = relCatEntry.numSlotsPerBlk;
    blockBuffer.setHeader(&blockheader);
    

    /*
        set block's slot map with all slots marked as free
        (i.e. store SLOT_UNOCCUPIED for all the entries)
        (use RecBuffer::setSlotMap() function)
    */
    unsigned char *slotMap = (unsigned char *)malloc(sizeof(unsigned char) * relCatEntry.numSlotsPerBlk);
    for (int slot = 0; slot < relCatEntry.numSlotsPerBlk; slot++) {
      slotMap[slot] = SLOT_UNOCCUPIED;
    }
    blockBuffer.setSlotMap(slotMap);

    // if prevBlockNum != -1
    if (prevBlockNum != -1) {
      // create a RecBuffer object for prevBlockNum
      // get the header of the block prevBlockNum and
      // update the rblock field of the header to the new block
      // number i.e. rec_id.block
      // (use BlockBuffer::setHeader() function)
      RecBuffer prevBuffer(prevBlockNum);
      HeadInfo prevHeader;
      prevBuffer.getHeader(&prevHeader);
      prevHeader.rblock = blockNum;
      prevBuffer.setHeader(&prevHeader);
    } else // else
    {
      // update first block field in the relation catalog entry to the
      // new block (using RelCacheTable::setRelCatEntry() function)
      relCatEntry.firstBlk = rec_id.block;
      RelCacheTable::setRelCatEntry(relId, &relCatEntry);
    }
    relCatEntry.lastBlk = rec_id.block;
    RelCacheTable::setRelCatEntry(relId, &relCatEntry);

    // update last block field in the relation catalog entry to the
    // new block (using RelCacheTable::setRelCatEntry() function)
  }

  // create a RecBuffer object for rec_id.block
  // insert the record into rec_id'th slot using RecBuffer.setRecord())
  RecBuffer blockBuffer(rec_id.block);
 int ret= blockBuffer.setRecord(record,rec_id.slot);
 if(ret!=SUCCESS){
  printf("Record not saved successfully.\n");
        exit(1);
 }
  /* update the slot map of the block by marking entry of the slot to
     which record was inserted as occupied) */
  // (ie store SLOT_OCCUPIED in free_slot'th entry of slot map)
  // (use RecBuffer::getSlotMap() and RecBuffer::setSlotMap() functions)
  unsigned char *slotMap=(unsigned char *)malloc(sizeof(unsigned char )*relCatEntry.numSlotsPerBlk);
  blockBuffer.getSlotMap(slotMap);
  slotMap[rec_id.slot]=SLOT_OCCUPIED;
  blockBuffer.setSlotMap(slotMap);
    
  // increment the numEntries field in the header of the block to
  // which record was inserted
  // (use BlockBuffer::getHeader() and BlockBuffer::setHeader() functions)
  HeadInfo header;
  blockBuffer.getHeader(&header);
  header.numEntries=header.numEntries+1;
  blockBuffer.setHeader(&header);
  // Increment the number of records field in the relation cache entry for
  // the relation. (use RelCacheTable::setRelCatEntry function)

  relCatEntry.numRecs++;
  
  RelCacheTable::setRelCatEntry(relId,&relCatEntry);

  return SUCCESS;
}