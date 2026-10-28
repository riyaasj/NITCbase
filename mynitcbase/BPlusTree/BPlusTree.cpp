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
    int ret = AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);


    AttrCatEntry attrCatEntry;
    ret = AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);


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
    if (blockHeader.pblock != -1) // (check pblock in header)
    {  
        // insert the middle value from `internalEntries` into the parent block
        // using the insertIntoInternal() function (recursively).

        // create a struct InternalEntry with lChild = current block, rChild = newRightBlk
        // and attrVal = internalEntries[MIDDLE_INDEX_INTERNAL].attrVal
        // and pass it as argument to the insertIntoInternalFunction as follows

        InternalEntry middleEntry;
        middleEntry.lChild = intBlockNum, middleEntry.rChild = newRightBlk;

        // the middle value will be at index 50 (given by constant MIDDLE_INDEX_INTERNAL)
        middleEntry.attrVal = internalEntries[MIDDLE_INDEX_INTERNAL].attrVal;

        // insertIntoInternal(relId, attrName, parent of current block, new internal entry)
        return insertIntoInternal(relId, attrName, blockHeader.pblock, middleEntry);
        // // if (ret == E_DISKFULL) return E_DISKFULL;
    } 
    else 
    {
        // the current block was the root block and is now split. a new internal index
        // block needs to be allocated and made the root of the tree.
        // To do this, call the createNewRoot() function with the following arguments

        return createNewRoot(relId, attrName, internalEntries[MIDDLE_INDEX_INTERNAL].attrVal, 
                                intBlockNum, newRightBlk);


        // createNewRoot(relId, attrName,
        //               internalEntries[MIDDLE_INDEX_INTERNAL].attrVal,
        //               current block, new right block)

        //// if (ret == E_DISKFULL) return E_DISKFULL;
    }

    // if either of the above calls returned an error (E_DISKFULL), then return that
    // else return SUCCESS
    return SUCCESS;
}

int BPlusTree::splitInternal(int intBlockNum, InternalEntry internalEntries[]) 
{
    // declare rightBlk, an instance of IndInternal using constructor 1 to obtain new
    // internal index block that will be used as the right block in the splitting
    IndInternal rightBlock;

    // declare leftBlk, an instance of IndInternal using constructor 2 to read from
    // the existing internal index block
    IndInternal leftBlock (intBlockNum);

    int leftBlockNum = leftBlock.getBlockNum();
    int rightBlockNum = rightBlock.getBlockNum();

    // if newly allocated block has blockNum E_DISKFULL 
    if (rightBlockNum == E_DISKFULL)
    {
        //(failed to obtain a new internal index block because the disk is full)
        return E_DISKFULL;
    }

    HeadInfo leftBlockHeader, rightBlockHeader;

    // get the headers of left block and right block using BlockBuffer::getHeader()
    leftBlock.getHeader(&leftBlockHeader);
    rightBlock.getHeader(&rightBlockHeader);

    // set rightBlkHeader with the following values
    // - number of entries = (MAX_KEYS_INTERNAL)/2 = 50
    // - pblock = pblock of leftBlk
    // and update the header of rightBlk using BlockBuffer::setHeader()
    
    rightBlockHeader.numEntries = MAX_KEYS_INTERNAL / 2;
    rightBlockHeader.pblock = leftBlockHeader.pblock;

    rightBlock.setHeader(&rightBlockHeader);

    // set leftBlkHeader with the following values
    // - number of entries = (MAX_KEYS_INTERNAL)/2 = 50
    // - rblock = rightBlkNum
    // and update the header using BlockBuffer::setHeader()

    leftBlockHeader.numEntries = MAX_KEYS_INTERNAL / 2;
    leftBlockHeader.rblock = rightBlockNum;

    leftBlock.setHeader(&leftBlockHeader);

    /*
    - set the first 50 entries of leftBlk = index 0 to 49 of internalEntries
      array
    - set the first 50 entries of newRightBlk = entries from index 51 to 100
      of internalEntries array using IndInternal::setEntry().
      (index 50 will be moving to the parent internal index block)
    */

    for (int entryindex = 0; entryindex < MIDDLE_INDEX_INTERNAL; entryindex++)
    {
        leftBlock.setEntry(&internalEntries[entryindex], entryindex);
        rightBlock.setEntry(&internalEntries[entryindex + MIDDLE_INDEX_INTERNAL + 1], entryindex);
    }

    /* block type of a child of any entry of the internalEntries array */;
    //            (use StaticBuffer::getStaticBlockType())
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
    // get the attribute cache entry corresponding to attrName
    // using AttrCacheTable::getAttrCatEntry().
    AttrCatEntry attrCatEntryBuffer;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntryBuffer);

    // declare newRootBlk, an instance of IndInternal using appropriate constructor
    // to allocate a new internal index block on the disk
    IndInternal newRootBlock;

    int newRootBlkNum = newRootBlock.getBlockNum();

    if (newRootBlkNum == E_DISKFULL) 
    {
        // (failed to obtain an empty internal index block because the disk is full)

        // Using bPlusDestroy(), destroy the right subtree, rooted at rChild.
        // This corresponds to the tree built up till now that has not yet been
        // connected to the existing B+ Tree

        BPlusTree::bPlusDestroy(rChild);

        return E_DISKFULL;
    }

    // update the header of the new block with numEntries = 1 using
    // BlockBuffer::getHeader() and BlockBuffer::setHeader()
    HeadInfo blockHeader;
    newRootBlock.getHeader(&blockHeader);

    blockHeader.numEntries = 1;
    newRootBlock.setHeader(&blockHeader);

    // create a struct InternalEntry with lChild, attrVal and rChild from the
    // arguments and set it as the first entry in newRootBlk using IndInternal::setEntry()
    InternalEntry internalentry;
    internalentry.lChild = lChild, internalentry.rChild = rChild, internalentry.attrVal = attrVal;

    newRootBlock.setEntry(&internalentry, 0);

    // declare BlockBuffer instances for the `lChild` and `rChild` blocks using
    // appropriate constructor and update the pblock of those blocks to `newRootBlkNum`
    // using BlockBuffer::getHeader() and BlockBuffer::setHeader()
    BlockBuffer leftChildBlock (lChild);
    BlockBuffer rightChildBlock (rChild);

    HeadInfo leftChildHeader, rightChildHeader;
    leftChildBlock.getHeader(&leftChildHeader);
    rightChildBlock.getHeader(&rightChildHeader);

    leftChildHeader.pblock = newRootBlkNum;
    rightChildHeader.pblock = newRootBlkNum;

    leftChildBlock.setHeader(&leftChildHeader);
    rightChildBlock.setHeader(&rightChildHeader);

    // update rootBlock = newRootBlkNum for the entry corresponding to `attrName`
    // in the attribute cache using AttrCacheTable::setAttrCatEntry().
    attrCatEntryBuffer.rootBlock = newRootBlkNum;
    AttrCacheTable::setAttrCatEntry(relId, attrName,  &attrCatEntryBuffer);

    return SUCCESS;
}

int BPlusTree::bPlusDestroy(int rootBlockNum) {
    // if (/*rootBlockNum lies outside the valid range [0,DISK_BLOCKS-1]*/) 
    if (rootBlockNum < 0 || rootBlockNum >= DISK_BLOCKS)
        return E_OUTOFBOUND;

    int type = StaticBuffer::getStaticBlockType(rootBlockNum); // type of block using StaticBuffer::getStaticBlockType() 

    if (type == IND_LEAF) 
    {
        // declare an instance of IndLeaf for rootBlockNum using appropriate constructor
        IndLeaf leafBlock (rootBlockNum);

        // release the block using BlockBuffer::releaseBlock().
        leafBlock.releaseBlock();

        return SUCCESS;

    } 
    else if (type == IND_INTERNAL) 
    {
        // declare an instance of IndInternal for rootBlockNum using appropriate constructor
        IndInternal internalBlock (rootBlockNum);

        // load the header of the block using BlockBuffer::getHeader().
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

