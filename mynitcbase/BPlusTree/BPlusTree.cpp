#include "BPlusTree.h"

#include <cstring>

inline bool operator == (RecId lhs, RecId rhs) {
	return (lhs.block == rhs.block && lhs.slot == rhs.slot);
}

inline bool operator != (RecId lhs, RecId rhs) {
	return (lhs.block != rhs.block || lhs.slot != rhs.slot);
}

inline bool operator == (IndexId lhs, IndexId rhs) {
	return (lhs.block == rhs.block && lhs.index == rhs.index);
}

inline bool operator != (IndexId lhs, IndexId rhs) {
	return (lhs.block != rhs.block || lhs.index != rhs.index);
}



RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int op) {

    //find the starting position
    int block = -1, index = -1;
    IndexId searchIndex;
    AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);


    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);


    //search is done for the first time
    if (searchIndex == IndexId{-1, -1}){


        block = attrCatEntry.rootBlock;
        index = 0;

        //attribute is not indexed
        if (block == -1) return RecId{-1, -1};

    } 

    else {
        
        block = searchIndex.block;
        index = searchIndex.index + 1;


        IndLeaf leaf(block);
        HeadInfo leafHead;
        leaf.getHeader(&leafHead);

        //no more entries in the block
        if (index >= leafHead.numEntries) {

            block = leafHead.rblock;
            index = 0;

            //search is done
            if (block == -1) {
                return RecId{-1, -1};
            }
        }
    }

    //traverse through the internal nodes
    while(StaticBuffer::getStaticBlockType(block) == IND_INTERNAL){  


        IndInternal internalBlk(block);
        
        HeadInfo intHead;
        internalBlk.getHeader(&intHead);

        InternalEntry intEntry;


        //for these operations the first entry satifying the condition(if exists) will be the first one
        if (op == NE || op == LT || op == LE){
            
            internalBlk.getEntry(&intEntry, 0);
            block = intEntry.lChild;

        } 
        
        //for these we have to find the first entry satisfying
        else{
            
            int entryindex = 0;

            //search the first entry satisying the condition in the bloxk
            while (entryindex < intHead.numEntries){

                internalBlk.getEntry(&intEntry, entryindex);
                int cmpVal = compareAttrs(intEntry.attrVal, attrVal, attrCatEntry.attrType);

                //if found
                if ((op == EQ && cmpVal >= 0) || (op == GE && cmpVal >= 0) || (op == GT && cmpVal > 0)) break;

                entryindex++;
            }

            //if entry satisying the condition is found in this block -> move to ist left
            if (entryindex < intHead.numEntries){
                block = intEntry.lChild;
            }

            //move to the right
            else{
                block = intEntry.rChild;
            }

            index = 0;
        }
    }

    //leaf node
    while (block != -1) {
        
        IndLeaf leafBlk(block);

        HeadInfo leafHead;
        leafBlk.getHeader(&leafHead);

        Index leafEntry;

        while (index < leafHead.numEntries){

            leafBlk.getEntry(&leafEntry, index);

            int cmpVal = compareAttrs(leafEntry.attrVal, attrVal, attrCatEntry.attrType); 

            //if the entry satisfies
            if ((op == EQ && cmpVal == 0) || (op == LE && cmpVal <= 0) || (op == GE && cmpVal >= 0) || (op == LT && cmpVal < 0) || (op == GT && cmpVal > 0) || (op == NE && cmpVal != 0)){

                searchIndex = IndexId{block, index};
                AttrCacheTable::setSearchIndex(relId, attrName, &searchIndex);
                return RecId{leafEntry.block, leafEntry.slot};
            } 


            //if you find the first value that is greater than the comparison value then stop searching for these operands
            else if ((op == EQ || op == LE || op == LT) && cmpVal > 0) {
                return RecId {-1, -1};
            }

            ++index;
        }

        //???
        if (op != NE && op != GE && op != GT) {
            break;
        }

        block = leafHead.rblock;
        index = 0;
    }

    // no entry satisying the op was found; return the recId {-1,-1}
    return RecId{-1, -1};
}



int BPlusTree::bPlusCreate(int relId, char attrName[ATTR_SIZE]) {

    //not allowed to create index on relcat or attrcat
    if (relId == RELCAT_RELID || relId == ATTRCAT_RELID)
        return E_NOTPERMITTED;



    AttrCatEntry attrCatEntryBuffer;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntryBuffer);
    if (ret != SUCCESS) return ret;


    //if attribute already indexed
    if (attrCatEntryBuffer.rootBlock != -1)
        return SUCCESS;

    
    //get a free leaf block
    IndLeaf rootBlockBuf;
    int rootBlock = rootBlockBuf.getBlockNum();


    //disk is full
    if (rootBlock == E_DISKFULL) return E_DISKFULL;


    //update attribute catalog cache entry with rootBlock
    attrCatEntryBuffer.rootBlock = rootBlock;
    AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntryBuffer);



    RelCatEntry relCatEntryBuffer;
    RelCacheTable::getRelCatEntry(relId, &relCatEntryBuffer);
    
    int block = relCatEntryBuffer.firstBlk;


    //traverse the entire relation and insert into the B+ tree
    while (block != -1){

        RecBuffer blockBuffer(block);

        unsigned char slotmap[relCatEntryBuffer.numSlotsPerBlk];
        blockBuffer.getSlotMap(slotmap);
        
        for (int slot = 0; slot < relCatEntryBuffer.numSlotsPerBlk; slot++){
            if (slotmap[slot] == SLOT_OCCUPIED){

                Attribute record[relCatEntryBuffer.numAttrs];
                blockBuffer.getRecord(record, slot);

                RecId recId = RecId{block, slot};

                
                ret = bPlusInsert(relId, attrName, record[attrCatEntryBuffer.offset], recId);

                if (ret == E_DISKFULL) {
                    return E_DISKFULL;
                }
            }
        }
        

        HeadInfo blockHeader;
        blockBuffer.getHeader(&blockHeader);

        block = blockHeader.rblock;
    }

    return SUCCESS;
}




int BPlusTree::bPlusInsert(int relId, char attrName[ATTR_SIZE], Attribute attrVal, RecId recId) {


    AttrCatEntry attrCatEntryBuffer;
    int ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntryBuffer);
    if (ret != SUCCESS) return ret;

    //get the rootblock
    int rootBlock = attrCatEntryBuffer.rootBlock; 

    //no rootblock?
    if (rootBlock == -1) return E_NOINDEX;


    //find the leaf block to insert
    int leafBlkNum = findLeafToInsert(rootBlock, attrVal, attrCatEntryBuffer.attrType);


    //make the index entry
    Index indexEntry; 
    indexEntry.attrVal = attrVal;
    indexEntry.block = recId.block;
    indexEntry.slot = recId.slot;
    
    //if disk is full -> destroy the existing B+ tree and update the attribute catalog entry
    if (insertIntoLeaf(relId, attrName, leafBlkNum, indexEntry) == E_DISKFULL){

        BPlusTree::bPlusDestroy(rootBlock);

        attrCatEntryBuffer.rootBlock = -1;
        AttrCacheTable::setAttrCatEntry(relId, attrName, &attrCatEntryBuffer);

        return E_DISKFULL;
    }

    return SUCCESS;
}



int BPlusTree::findLeafToInsert(int rootBlock, Attribute attrVal, int attrType) {

    int blockNum = rootBlock;


    //while block is internal
    while (StaticBuffer::getStaticBlockType(blockNum) != IND_LEAF){  

        IndInternal internalBlock (blockNum);
        HeadInfo blockHeader;
        internalBlock.getHeader(&blockHeader);


        int index = 0;
        while (index < blockHeader.numEntries){
            InternalEntry entry;
            internalBlock.getEntry(&entry, index);

            if (compareAttrs(attrVal, entry.attrVal, attrType) <= 0){
                break;
            }

            index++;
        }


        //attrVal is greater than all the entries in the block -> move right 
        if (index == blockHeader.numEntries){

            InternalEntry entry;
            internalBlock.getEntry(&entry, blockHeader.numEntries-1);
            blockNum = entry.rChild;

        } 

        //entry greater than or equal to attrVal is found -> move left
        else {

            InternalEntry entry;
            internalBlock.getEntry(&entry, index);
            blockNum = entry.lChild;
        }
    }

    return blockNum;
}



int BPlusTree::insertIntoLeaf(int relId, char attrName[ATTR_SIZE], int leafBlockNum, Index indexEntry) {


    AttrCatEntry attrCatEntryBuffer;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntryBuffer);

    IndLeaf leafBlock (leafBlockNum);

    HeadInfo blockHeader;
    leafBlock.getHeader(&blockHeader);


    //temporary array of indexes
    Index indices[blockHeader.numEntries + 1];

    
    bool inserted = false;
    for (int entryindex = 0; entryindex < blockHeader.numEntries; entryindex++){

        Index entry;
        leafBlock.getEntry(&entry, entryindex);

        //if the entry is less than or equal to the indexEntry, add it to the indices array
        if (compareAttrs(entry.attrVal, indexEntry.attrVal, attrCatEntryBuffer.attrType) <= 0){
            indices[entryindex] = entry;
        }

        else{

            //add indexEntry
            indices[entryindex] = indexEntry;
            inserted = true;

            //add the rest
            for (entryindex++; entryindex <= blockHeader.numEntries; entryindex++){
                leafBlock.getEntry(&entry, entryindex - 1);
                indices[entryindex] = entry;
            }
            break;
        }
    }

    // adding the last element in indices
    if (!inserted) indices[blockHeader.numEntries] = indexEntry;

    //leaf block has not reached max limit
    if (blockHeader.numEntries < MAX_KEYS_LEAF) {

        //increment blockHeader.numEntries and update the header of block
        blockHeader.numEntries++;
        leafBlock.setHeader(&blockHeader);

        //populate the leaf block with the entries in the indices array
        for (int indicesIt = 0; indicesIt < blockHeader.numEntries; indicesIt++){
            leafBlock.setEntry(&indices[indicesIt], indicesIt);
        }

        return SUCCESS;
    }


    //leaf block has reached max limit
    int newRightBlk = splitLeaf(leafBlockNum, indices);

    //disk is full
    if (newRightBlk == E_DISKFULL) return E_DISKFULL;


    //if current leaf block is not the root
    if (blockHeader.pblock != -1){

        //insert the middle entry into parent block, left child = current block, right child = new right block
        InternalEntry middleEntry;
        middleEntry.attrVal = indices[MIDDLE_INDEX_LEAF].attrVal, 
        middleEntry.lChild = leafBlockNum, middleEntry.rChild = newRightBlk;

        return insertIntoInternal(relId, attrName, blockHeader.pblock, middleEntry);
    }
    
    //current leaf block is the root -> create new root
    else {
        if(createNewRoot(relId, attrName, indices[MIDDLE_INDEX_LEAF].attrVal, leafBlockNum, newRightBlk) == E_DISKFULL)
            return E_DISKFULL;
    }


    return SUCCESS;
}





int BPlusTree::splitLeaf(int leafBlockNum, Index indices[]) {
    
    //get a new leaf block for the right child
    IndLeaf rightBlock;


    IndLeaf leftBlock(leafBlockNum);

    int rightBlockNum = rightBlock.getBlockNum();
    int leftBlockNum = leftBlock.getBlockNum();

    if (rightBlockNum == E_DISKFULL) {
        return E_DISKFULL;
    }


    HeadInfo leftBlockHeader, rightBlockHeader;
    leftBlock.getHeader(&leftBlockHeader);
    rightBlock.getHeader(&rightBlockHeader);


    
    rightBlockHeader.blockType = leftBlockHeader.blockType, 
    rightBlockHeader.numEntries = (MAX_KEYS_LEAF + 1) / 2;
    rightBlockHeader.pblock = leftBlockHeader.pblock, 
    rightBlockHeader.lblock = leftBlockNum, 
    rightBlockHeader.rblock = leftBlockHeader.rblock;

    rightBlock.setHeader(&rightBlockHeader);



    leftBlockHeader.numEntries = (MAX_KEYS_LEAF+1)/2;
    leftBlockHeader.rblock = rightBlockNum;

    leftBlock.setHeader(&leftBlockHeader);


    //first 32 in left
    //last 32 in right
    for (int entryindex = 0; entryindex <= MIDDLE_INDEX_LEAF; entryindex++){
        leftBlock.setEntry(&indices[entryindex], entryindex);
        rightBlock.setEntry(&indices[entryindex + MIDDLE_INDEX_LEAF + 1], entryindex);
    }

    return rightBlockNum;
}




int BPlusTree::insertIntoInternal(int relId, char attrName[ATTR_SIZE], int intBlockNum, InternalEntry intEntry) {

    AttrCatEntry attrCatEntryBuffer;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntryBuffer);


    IndInternal internalBlock (intBlockNum);

    HeadInfo blockHeader;
    internalBlock.getHeader(&blockHeader);

    //temporary array of internal entries
    InternalEntry internalEntries[blockHeader.numEntries + 1];

    

    //insert in correct position
    int insertedIndex = -1;
    for (int entryindex = 0; entryindex < blockHeader.numEntries; entryindex++){
        InternalEntry internalBlockEntry;
        internalBlock.getEntry(&internalBlockEntry, entryindex);

        if (compareAttrs(internalBlockEntry.attrVal, intEntry.attrVal, attrCatEntryBuffer.attrType) <= 0){
            internalEntries[entryindex] = internalBlockEntry;
        }
        else {

            internalEntries[entryindex] = intEntry;
            insertedIndex = entryindex;

            for (entryindex++; entryindex <= blockHeader.numEntries; entryindex++){

                internalBlock.getEntry(&internalBlockEntry, entryindex - 1);
                internalEntries[entryindex] = internalBlockEntry;
            }

            break;
        }
    }


    //if the entry is greater than all the entries in the block, add it to the end of the array
    if (insertedIndex == -1) {
        internalEntries[blockHeader.numEntries] = intEntry;
        insertedIndex = blockHeader.numEntries;
    }

    //if there is an entry to the left, rchild of leftentry = lchild of this entry
    if (insertedIndex > 0){
        internalEntries[insertedIndex - 1].rChild = intEntry.lChild;
    }


    //if there is an entry to the right, lchild of rightentry = rchild of this entry
    if (insertedIndex < blockHeader.numEntries){
        internalEntries[insertedIndex + 1].lChild = intEntry.rChild;
    }


    //internal block has not reached max limit
    if (blockHeader.numEntries < MAX_KEYS_INTERNAL) {

        // increment blockheader.numEntries and update the header of intBlk
        blockHeader.numEntries++;
        internalBlock.setHeader(&blockHeader);

        //populate the internal block with the entries in the internalEntries array
        for (int entryindex = 0; entryindex < blockHeader.numEntries; entryindex++)
            internalBlock.setEntry(&internalEntries[entryindex], entryindex);

        return SUCCESS;
    }

    
    //internal block has reached max limit -> split the internal block and insert the middle entry into the parent block
    int newRightBlk = splitInternal(intBlockNum, internalEntries);


    //??
    if (newRightBlk == E_DISKFULL){

        BPlusTree::bPlusDestroy(intEntry.rChild);

        return E_DISKFULL;
    }

    // if the current block was not the root  
    if (blockHeader.pblock != -1){  
 
        //insert middle entry to parent block 
        InternalEntry middleEntry;
        middleEntry.lChild = intBlockNum, middleEntry.rChild = newRightBlk;
        middleEntry.attrVal = internalEntries[MIDDLE_INDEX_INTERNAL].attrVal;

        return insertIntoInternal(relId, attrName, blockHeader.pblock, middleEntry);

    } 

    // current block was the root -> create new root
    else{
        return createNewRoot(relId, attrName, internalEntries[MIDDLE_INDEX_INTERNAL].attrVal, intBlockNum, newRightBlk);
    }


    return SUCCESS;
}



int BPlusTree::splitInternal(int intBlockNum, InternalEntry internalEntries[]) {

    //new right block
    IndInternal rightBlock;


    IndInternal leftBlock(intBlockNum);

    int leftBlockNum = leftBlock.getBlockNum();
    int rightBlockNum = rightBlock.getBlockNum();


    //disk is full
    if (rightBlockNum == E_DISKFULL){

        return E_DISKFULL;
    }

    HeadInfo leftBlockHeader, rightBlockHeader;

    leftBlock.getHeader(&leftBlockHeader);
    rightBlock.getHeader(&rightBlockHeader);

    
    rightBlockHeader.numEntries = MAX_KEYS_INTERNAL / 2;
    rightBlockHeader.pblock = leftBlockHeader.pblock;

    rightBlock.setHeader(&rightBlockHeader);



    leftBlockHeader.numEntries = MAX_KEYS_INTERNAL / 2;
    leftBlockHeader.rblock = rightBlockNum;

    leftBlock.setHeader(&leftBlockHeader);

    
    //first 50 in left
    //last 50 in right
    for (int entryindex = 0; entryindex < MIDDLE_INDEX_INTERNAL; entryindex++){
        leftBlock.setEntry(&internalEntries[entryindex], entryindex);
        rightBlock.setEntry(&internalEntries[entryindex + MIDDLE_INDEX_INTERNAL + 1], entryindex);
    }

    //??
    int type = StaticBuffer::getStaticBlockType(internalEntries[0].lChild);

    // for (/* each child block of the new right block */) 
    BlockBuffer blockbuffer (internalEntries[MIDDLE_INDEX_INTERNAL+1].lChild);

    HeadInfo blockHeader;
    blockbuffer.getHeader(&blockHeader);

    blockHeader.pblock = rightBlockNum;
    blockbuffer.setHeader(&blockHeader);

    for (int entryindex = 0; entryindex < MIDDLE_INDEX_INTERNAL; entryindex++)
    {
        // declare an instance of BlockBuffer to access the child block using
        // constructor 2
        BlockBuffer blockbuffer (internalEntries[entryindex + MIDDLE_INDEX_INTERNAL+1].rChild);

        // update pblock of the block to rightBlkNum using BlockBuffer::getHeader()
        // and BlockBuffer::setHeader().
        // HeadInfo blockHeader;
        blockbuffer.getHeader(&blockHeader);

        blockHeader.pblock = rightBlockNum;
        blockbuffer.setHeader(&blockHeader);
    }

    return rightBlockNum; 
}



int BPlusTree::createNewRoot(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int lChild, int rChild) {


    AttrCatEntry attrCatEntryBuffer;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntryBuffer);


    IndInternal newRootBlock;

    int newRootBlkNum = newRootBlock.getBlockNum();

    if (newRootBlkNum == E_DISKFULL) {
        BPlusTree::bPlusDestroy(rChild);
        return E_DISKFULL;
    }


    //update header of the new root block
    HeadInfo blockHeader;
    newRootBlock.getHeader(&blockHeader);

    blockHeader.numEntries = 1;
    newRootBlock.setHeader(&blockHeader);


    //add the entry
    InternalEntry internalentry;
    internalentry.lChild = lChild, internalentry.rChild = rChild, internalentry.attrVal = attrVal;
    newRootBlock.setEntry(&internalentry, 0);



    BlockBuffer leftChildBlock (lChild);
    BlockBuffer rightChildBlock (rChild);


    //update headers of the left and right child blocks
    HeadInfo leftChildHeader, rightChildHeader;
    leftChildBlock.getHeader(&leftChildHeader);
    rightChildBlock.getHeader(&rightChildHeader);

    leftChildHeader.pblock = newRootBlkNum;
    rightChildHeader.pblock = newRootBlkNum;

    leftChildBlock.setHeader(&leftChildHeader);
    rightChildBlock.setHeader(&rightChildHeader);


    //update the rootBlock of the attribute catalog entry of the attribute
    attrCatEntryBuffer.rootBlock = newRootBlkNum;
    AttrCacheTable::setAttrCatEntry(relId, attrName,  &attrCatEntryBuffer);

    return SUCCESS;
}


int BPlusTree::bPlusDestroy(int rootBlockNum) {


    if (rootBlockNum < 0 || rootBlockNum >= DISK_BLOCKS)
        return E_OUTOFBOUND;

    int type = StaticBuffer::getStaticBlockType(rootBlockNum);

    if (type == IND_LEAF) {

        IndLeaf leafBlock (rootBlockNum);
        leafBlock.releaseBlock();
        return SUCCESS;

    } 
    else if (type == IND_INTERNAL){

        IndInternal internalBlock (rootBlockNum);

        HeadInfo blockHeader;
        internalBlock.getHeader(&blockHeader);

        /*iterate through all the entries of the internalBlk and destroy the lChild
        of the first entry and rChild of all entries using BPlusTree::bPlusDestroy().
        (the rchild of an entry is the same as the lchild of the next entry.
         take care not to delete overlapping children more than once ) */
        
        InternalEntry blockEntry;
        internalBlock.getEntry (&blockEntry, 0);

        BPlusTree::bPlusDestroy(blockEntry.lChild);

        for (int entry = 0; entry < blockHeader.numEntries; entry++) {
            internalBlock.getEntry (&blockEntry, entry);
            BPlusTree::bPlusDestroy(blockEntry.rChild);
        }

        // release the block using BlockBuffer::releaseBlock().
        internalBlock.releaseBlock();

        return SUCCESS;

    } 
    else // (block is not an index block.)
        return E_INVALIDBLOCK;
}

