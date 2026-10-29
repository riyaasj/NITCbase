#include "BlockAccess.h"
#include <cstring>
#include <iostream>

int BlockAccess::comparisonCount = 0;

void BlockAccess::resetComparisonCount() {
    comparisonCount = 0;
}

void BlockAccess::incrementComparisonCount() {
    comparisonCount++;
}

int BlockAccess::getComparisonCount() {
    return comparisonCount;
}

RecId BlockAccess::linearSearch(int relId, char attrName[ATTR_SIZE], union Attribute attrVal, int op) {

    //previous record id to keep track of the last record found
    //(use RelCacheTable::getSearchIndex() function)
    RecId prevRecId = {-1, -1};
    RelCacheTable::getSearchIndex(relId, &prevRecId);

    // let block and slot denote the record id of the record being currently checked
    
    int block,slot;
    if(prevRecId.block == -1 && prevRecId.slot == -1) {
        RelCatEntry relCatEntry;
        RelCacheTable::getRelCatEntry(relId, &relCatEntry);
        block = relCatEntry.firstBlk;
        slot = 0;
    }
    else {
        block = prevRecId.block;
        slot = prevRecId.slot + 1;
    }

    /* The following code searches for the next record in the relation
       that satisfies the given condition
       We start from the record id (block, slot) and iterate over the remaining
       records of the relation
    */
    RelCatEntry relCatBuffer;
    RelCacheTable::getRelCatEntry(relId, &relCatBuffer);
    while(block != -1){

        RecBuffer recBuffer(block);
        HeadInfo head;
        recBuffer.getHeader(&head);

        unsigned char *slotMap = (unsigned char*)malloc(sizeof(unsigned char) * head.numSlots);
        recBuffer.getSlotMap(slotMap);

        if(slot >= head.numSlots) {
            block = head.rblock;
            slot = 0;
            continue;
        }

        if(slotMap[slot] == SLOT_UNOCCUPIED) {
            slot++;
            continue;
        }

    
        Attribute *record = (Attribute *)malloc(sizeof(Attribute) * head.numAttrs);
        recBuffer.getRecord(record, slot);

        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
        int attrOffset = attrCatEntry.offset;
        BlockAccess::incrementComparisonCount();
        int cmpVal = compareAttrs(record[attrOffset], attrVal, attrCatEntry.attrType);

         if((op == NE && cmpVal != 0 ) ||(op == LT && cmpVal < 0) ||(op == LE && cmpVal <= 0) ||(op == EQ && cmpVal == 0) ||(op == GT && cmpVal > 0) ||(op == GE && cmpVal >= 0)) {
            RecId newSearchIndex;
            newSearchIndex.block = block;
            newSearchIndex.slot = slot;
            RelCacheTable::setSearchIndex(relId, &newSearchIndex);
            return RecId{block, slot};
        }   
        slot++;
    }
    return RecId{-1,-1};
   }

    int BlockAccess::renameRelation(char oldRelName[ATTR_SIZE], char newRelName[ATTR_SIZE]) {

    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    Attribute newRelationName;
    strcpy(newRelationName.sVal,newRelName);
    RecId searchRes = linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, newRelationName, EQ);
    if(searchRes.block != -1 && searchRes.slot != -1) {
        return E_RELEXIST;
    }

    RelCacheTable::resetSearchIndex(RELCAT_RELID);
    Attribute oldRelationName;
    strcpy(oldRelationName.sVal,oldRelName);
    RecId searchResOld = linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, oldRelationName, EQ);
    if(searchResOld.block == -1 && searchResOld.slot == -1) {
        return E_RELNOTEXIST;
    }

    RecBuffer relCatBuffer(searchResOld.block);
    Attribute record[RELCAT_NO_ATTRS];
    relCatBuffer.getRecord(record,searchResOld.slot);
    record[RELCAT_REL_NAME_INDEX]=newRelationName;
    relCatBuffer.setRecord(record,searchResOld.slot);

    /*
    update all the attribute catalog entries in the attribute catalog corresponding
    to the relation with relation name oldName to the relation name newName
    */
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    for(int i = 0; i < record[RELCAT_NO_ATTRIBUTES_INDEX].nVal; i++) {
        RecId searchIndex = linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, oldRelationName, EQ);

        RecBuffer attrBlock(searchIndex.block);
        Attribute attrRecord[ATTRCAT_NO_ATTRS];
        attrBlock.getRecord(attrRecord, searchIndex.slot);
        strcpy(attrRecord[ATTRCAT_REL_NAME_INDEX].sVal, newRelName);
        attrBlock.setRecord(attrRecord, searchIndex.slot);
    }
    return SUCCESS;
   }

   int BlockAccess::renameAttribute(char relName[ATTR_SIZE], char oldAttrName[ATTR_SIZE], char newAttrName[ATTR_SIZE]) {

     RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr;
    strcpy(relNameAttr.sVal, relName);
    RecId searchIndex = linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, relNameAttr, EQ);
    if(searchIndex.block == -1 && searchIndex.slot == -1) {
        return E_RELNOTEXIST;
    }
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

    
    RecId attrToRenameRecId = {-1,-1};
    Attribute attrCatEntryRecord[ATTRCAT_NO_ATTRS];

    while(true) {
        searchIndex = BlockAccess::linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, relNameAttr, EQ);
        if(searchIndex.block == -1 && searchIndex.slot == -1) {
            break;
        }

        RecBuffer attrBuffer(searchIndex.block);
        Attribute attrRecord[ATTRCAT_NO_ATTRS];

        attrBuffer.getRecord(attrRecord, searchIndex.slot);

        if(strcmp(attrRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, oldAttrName) == 0) {
            attrToRenameRecId = searchIndex;
            break;
        }
        if(strcmp(attrRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newAttrName) == 0) {
            return E_ATTREXIST;
        }
    }
    if(attrToRenameRecId.block == -1 && attrToRenameRecId.slot == -1) {
        return E_ATTRNOTEXIST;
    }

    RecBuffer attrBuffer(attrToRenameRecId.block);
    Attribute attrRecord[ATTRCAT_NO_ATTRS];
    attrBuffer.getRecord(attrRecord, attrToRenameRecId.slot);
    strcpy(attrRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newAttrName);
    attrBuffer.setRecord(attrRecord, attrToRenameRecId.slot);

    return SUCCESS;
}

int BlockAccess::insert(int relId, union Attribute *record){  

    //get the first block of the relation
    RelCatEntry relCatEntry;
    RelCacheTable::getRelCatEntry(relId, &relCatEntry);

    int blockNum = relCatEntry.firstBlk;

    RecId rec_id = {-1,-1};
    int numOfSlots = relCatEntry.numSlotsPerBlk;
    int numOfAttributes = relCatEntry.numAttrs;
    int prevBlockNum = -1;


    //traverse through the blocks to find a free slot
    while(blockNum != -1) {
        RecBuffer recBuffer(blockNum);
        HeadInfo header;
        recBuffer.getHeader(&header);
        unsigned char* slotMap = (unsigned char*)malloc(sizeof(unsigned char) * header.numSlots);
        recBuffer.getSlotMap(slotMap);

        for(int i = 0; i < header.numSlots; i++) {
            if(slotMap[i] == SLOT_UNOCCUPIED) {
                rec_id.block = blockNum;
                rec_id.slot = i;
                break;
            }
        }

        //if no free slot is found in the current block, move to the next block
        if(rec_id.slot != -1 && rec_id.block != -1) {
            break;
        }
        prevBlockNum = blockNum;
        blockNum = header.rblock;
    }


    // if no more free slot is found in existing record blocks
    if(rec_id.block == -1 && rec_id.slot == -1) {
        
        //relation catalog is only allowed one block
        if(relId == RELCAT_RELID) {
            return E_MAXRELATIONS;
        }


        // allocate a new block for the record
        RecBuffer blockBuffer;
        blockNum = blockBuffer.getBlockNum();


        if(blockNum == E_DISKFULL) {
            return E_DISKFULL;
        }

        rec_id.block = blockNum;
        rec_id.slot = 0;


        // setting the header of the new record block
        HeadInfo blockHeader;
        blockHeader.pblock = blockHeader.rblock = -1;
        blockHeader.blockType = REC;
        if(relCatEntry.numRecs == 0) {
            blockHeader.lblock = -1;
        } else blockHeader.lblock = prevBlockNum;
        blockHeader.numAttrs = relCatEntry.numAttrs;
        blockHeader.numEntries = 0;
        blockHeader.numSlots = relCatEntry.numSlotsPerBlk;
        blockBuffer.setHeader(&blockHeader);


        //setting the slotmap of the new record block
        unsigned char* slotMap = (unsigned char*)malloc(sizeof(unsigned char) * relCatEntry.numSlotsPerBlk);
        for(int i = 0; i < relCatEntry.numSlotsPerBlk; i++) {
            slotMap[i] = SLOT_UNOCCUPIED;
        }
        blockBuffer.setSlotMap(slotMap);

        //update header of the left block
        if(prevBlockNum != -1) {
            RecBuffer prevBuffer(prevBlockNum);
            HeadInfo prevHeader;
            prevBuffer.getHeader(&prevHeader);
            prevHeader.rblock = blockNum;
            prevBuffer.setHeader(&prevHeader);
        } 
        else {
            // first record block for the relation

            relCatEntry.firstBlk = rec_id.block;
            RelCacheTable::setRelCatEntry(relId, &relCatEntry);

        }

        relCatEntry.lastBlk = rec_id.block;
        RelCacheTable::setRelCatEntry(relId, &relCatEntry);
    }

    //insert the record
    RecBuffer buffer(rec_id.block);
    int ret = buffer.setRecord(record, rec_id.slot);
    if(ret != SUCCESS) {
        printf("Record save unsuccessful.\n");
        exit(FAILURE);
    }


    //update the slotmap
    unsigned char* slotMap = (unsigned char*)malloc(sizeof(unsigned char) * relCatEntry.numSlotsPerBlk);
    buffer.getSlotMap(slotMap);
    slotMap[rec_id.slot] = SLOT_OCCUPIED;
    buffer.setSlotMap(slotMap);


    // increment the numEntries field in the header of the block
    HeadInfo header;
    buffer.getHeader(&header);
    header.numEntries+=1;
    buffer.setHeader(&header);
    

    //update numRecs in the relation catalog entry of the relation
    relCatEntry.numRecs++;
    RelCacheTable::setRelCatEntry(relId, &relCatEntry);

    for(int attrOffset = 0; attrOffset < numOfAttributes; attrOffset++) {
        AttrCatEntry attrCatEntry;
        int ret = AttrCacheTable::getAttrCatEntry(relId, attrOffset, &attrCatEntry);
        if(ret != SUCCESS) {
            return ret;
        }

        if(attrCatEntry.rootBlock != -1) {
            ret = BPlusTree::bPlusInsert(relId, attrCatEntry.attrName, record[attrOffset], rec_id);
            if(ret != SUCCESS) {
                return ret;
            }
        }
    }

    return SUCCESS;
}

int BlockAccess::deleteRelation(char relName[ATTR_SIZE]) {

    //not allowed to delete Relation Catalog or Attribute Catalog
    if(strcmp(relName, RELCAT_RELNAME)==0 || strcmp(relName, ATTRCAT_RELNAME)==0) {
        return E_NOTPERMITTED;
    }

    RelCacheTable::resetSearchIndex(RELCAT_RELID);

    Attribute relNameAttr;
    strcpy(relNameAttr.sVal, relName);


    //check if relation exists
    RecId searchRes = linearSearch(RELCAT_RELID, (char*)RELCAT_ATTR_RELNAME, relNameAttr, EQ);
    if(searchRes.block == -1 && searchRes.slot == -1) {
        return E_RELNOTEXIST;
    }

    Attribute relCatEntryRecord[RELCAT_NO_ATTRS];
    RecBuffer relCatBuffer(searchRes.block);
    relCatBuffer.getRecord(relCatEntryRecord, searchRes.slot);

    int relblk = relCatEntryRecord[RELCAT_FIRST_BLOCK_INDEX].nVal;
    int nAttrs = relCatEntryRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal;

    // delete all the record blocks of the relation
    while(true){
        if(relblk == -1) {
            break;
        }

        RecBuffer recBuffer(relblk);
        HeadInfo header;
        recBuffer.getHeader(&header);
        int nextBlock = header.rblock;

        recBuffer.releaseBlock();
        relblk = nextBlock;
    }

    // delete all the attribute catalog entries of the relation
    RelCacheTable::resetSearchIndex(ATTRCAT_RELID);
    int numberOfAttributesDeleted = 0;

    while(true){
        RecId attrCatRecId;
        attrCatRecId = linearSearch(ATTRCAT_RELID, (char*)ATTRCAT_ATTR_RELNAME, relNameAttr, EQ);

        //no more attributes?
        if(attrCatRecId.block == -1 && attrCatRecId.slot == -1) {
            break;
        }
        numberOfAttributesDeleted++;

        RecBuffer attrBuffer(attrCatRecId.block);
        Attribute attrRecord[ATTRCAT_NO_ATTRS];
        attrBuffer.getRecord(attrRecord, attrCatRecId.slot);

        int rootBlock = attrRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal;

        HeadInfo attrHeader;
        attrBuffer.getHeader(&attrHeader);

        //update the slotmap of the attribute catalog block
        unsigned char* slotMap = (unsigned char*)malloc(sizeof(unsigned char) * attrHeader.numSlots);
        attrBuffer.getSlotMap(slotMap);
        slotMap[attrCatRecId.slot] = SLOT_UNOCCUPIED;
        attrBuffer.setSlotMap(slotMap);

        //update numEntries in the header of the attribute catalog block
        attrHeader.numEntries--;
        attrBuffer.setHeader(&attrHeader);


        //if that was the last attribute catalog entry of the relation
        if(attrHeader.numEntries == 0){

            //update the left block of the attribute catalog block
            RecBuffer prevBlock(attrHeader.lblock);
            HeadInfo leftHeader;
			prevBlock.getHeader(&leftHeader);

			leftHeader.rblock = attrHeader.rblock;
			prevBlock.setHeader(&leftHeader);

            if (attrHeader.rblock != INVALID_BLOCKNUM) {
                
				RecBuffer nextBlock (attrHeader.rblock);
				
				HeadInfo rightHeader;
				nextBlock.getHeader(&rightHeader);

				rightHeader.lblock = attrHeader.lblock;
				nextBlock.setHeader(&rightHeader);
            }

            else{
                RelCatEntry relCatEntry;
                RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relCatEntry);
                relCatEntry.lastBlk = attrHeader.lblock;
                RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &relCatEntry);
            }
            attrBuffer.releaseBlock();
        }

        if(rootBlock != -1){
            BPlusTree::bPlusDestroy(rootBlock);
        }

    }

    HeadInfo relCatHeader;
    relCatBuffer.getHeader(&relCatHeader);


    //update numEntries in the header of the relation catalog block
	relCatHeader.numEntries--;
	relCatBuffer.setHeader(&relCatHeader);

    //update the slotmap of the relation catalog block
	unsigned char slotmap [relCatHeader.numSlots];
	relCatBuffer.getSlotMap(slotmap);
	slotmap[searchRes.slot] = SLOT_UNOCCUPIED;
	relCatBuffer.setSlotMap(slotmap);


	RelCatEntry relCatEntryBuffer;
	RelCacheTable::getRelCatEntry(RELCAT_RELID, &relCatEntryBuffer);

	relCatEntryBuffer.numRecs--;
	RelCacheTable::setRelCatEntry(RELCAT_RELID, &relCatEntryBuffer);

	RelCacheTable::getRelCatEntry(ATTRCAT_RELID, &relCatEntryBuffer);
	relCatEntryBuffer.numRecs -= numberOfAttributesDeleted;
	RelCacheTable::setRelCatEntry(ATTRCAT_RELID, &relCatEntryBuffer);


    return SUCCESS;
}

int BlockAccess::search(int relId, Attribute *record, char attrName[ATTR_SIZE], Attribute attrVal, int op){

    RecId recId;


    //get attribute catalog entry of the attribute from attribute cache
    AttrCatEntry attrCatEntry;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);
    if(ret != SUCCESS){
        return ret; 
    }


    int rootBlock = attrCatEntry.rootBlock;

    //attribute is not indexed -> linear search
    if(rootBlock == -1){
        recId = BlockAccess::linearSearch(relId, attrName, attrVal, op);
    }

    //attribute in indexed -> b+ tree search
    else{
        recId = BPlusTree::bPlusSearch(relId, attrName, attrVal, op);
    }


    //if record not found
    if(recId.block == -1 && recId.slot == -1){
        return E_NOTFOUND;
    }

    //copy the record into record
    RecBuffer recBuffer(recId.block);
    recBuffer.getRecord(record, recId.slot);

    return SUCCESS;
}
    

    
  int BlockAccess::project(int relId, Attribute *record){

    //get the previous search index of the relation
    RecId prevRecId;
    RelCacheTable::getSearchIndex(relId, &prevRecId);

    int block, slot;

    //new projection operation
    if(prevRecId.block == -1 && prevRecId.slot == -1){
        
        //start from the first block 
        RelCatEntry relCatEntry;
        int ret = RelCacheTable::getRelCatEntry(relId, &relCatEntry);

        if(ret != SUCCESS){
            return ret;
        }

        block = relCatEntry.firstBlk;
        slot = 0;
    }
    else{
        // means the projection has already in progress.
        block = prevRecId.block;
        slot = prevRecId.slot + 1;
    }

    
    while(block != -1){
        
        RecBuffer recBuffer(block);

        struct HeadInfo head;
        recBuffer.getHeader(&head);

        unsigned char slotMap[head.numSlots];
        recBuffer.getSlotMap(slotMap);


        //no more slots in the current block, move to the next block
        if(slot >= head.numSlots){
            block = head.rblock;
            slot = 0;
        }

        // if slot is free.
        else if(slotMap[slot] == SLOT_UNOCCUPIED){
            slot++;
        }
        else{
            break; // next occupied slot and record has been found.
        }
    }

    //all records have been projected, return E_NOTFOUND
    if(block == -1){
        return E_NOTFOUND;
    }

    //store the record Id found
    RecId nextRecId{block, slot};

    //set the searchIndex to nextRecId
    RelCacheTable::setSearchIndex(relId, &nextRecId);

    RecBuffer recBuffer(nextRecId.block);
    recBuffer.getRecord(record, nextRecId.slot);

    return SUCCESS;

}