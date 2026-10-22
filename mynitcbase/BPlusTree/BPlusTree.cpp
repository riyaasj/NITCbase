#include "BPlusTree.h"

#include <cstring>


RecId BPlusTree::bPlusSearch(int relId, char attrName[ATTR_SIZE], Attribute attrVal, int op){
    

    //get the previous search index of the attribute
    IndexId searchIndex;
    AttrCacheTable::getSearchIndex(relId, attrName, &searchIndex);

    // get the attrCatEntry
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, attrName, &attrCatEntry);



    int block, index;

    //if search is for the first time
    if(searchIndex.block == -1 && searchIndex.index == -1){
        
        // start the search from the first entry of the root
        block = attrCatEntry.rootBlock;
        index = 0;

        // if attrName doesn't have a B+ tree
        if(block == -1){
            return RecId{-1,-1};
        }
    }
    else{
        
        block = searchIndex.block;
        index = searchIndex.index + 1; // search is resumed from the next index

        // load block into leaf
        IndLeaf leaf(block);

        // declare leafHead which will store the header of the leaf. Load the header
        HeadInfo leafHead;
        leaf.getHeader(&leafHead);

        // if the index is more than the no. of entries, search in the next leaf
        if(index >= leafHead.numEntries){
            block = leafHead.rblock;

            if(block == -1){
                // end of the linklist - search is done
                return RecId{-1, -1};
            }
        }
    }

    /**** Traverse through all the internal nodes ****/
    /*
        NOTE:
            - This section is needed when search starts from the root, (root is not leaf)

            If there was valid search index, then we are already at a leaf block, thus this section's fails in loop.
    */

    while(StaticBuffer::getStaticBlockType(block) == IND_INTERNAL){
        
        // load the block into internalBlk and get the block header
        IndInternal internalBlk(block);
        HeadInfo intHead;
        internalBlk.getHeader(&intHead);

        // declare the intEntry which will be used to store entry of internalBlk.
        InternalEntry intEntry;

        if(op == NE || op == LT || op == LE){
            /*
                - NE: need to search the entire linked list of leaf indices of the B+ Tree,
                starting from the leftmost leaf index. Thus, always move to the left.

                - LE or LT: the attribute values are arranged in ascending order in the
                leaf indices of the B+ Tree. Values that satisfy these conditions, if any
                exist, will be found in the left-most leaf index. Thus, always move to the
                left.
            */

            // load entry of first slot of the block into intEntry
            internalBlk.getEntry(&intEntry, 0);
            block = intEntry.lChild;

        }
        else{

            /*
                - EQ, GT, GE: move to the left child of the first entry that is greater
                than (or equal to) attrVal

                We are trying to find the first entry that satisfies the condition.
                Since the values are in ascending order we move to the left child which
                might contain more entries that satisfy the condition
            */

            /*
                Traverse through all entries of the internalBlk and find an entry that
                satisfies the condition:
                a) If op == EQ or GE, then intEntry.attrVal >= attrVal
                b) If op == GT, then intEntry.attrVal > attrVal
            */
            bool found = false;
            for(int i = 0; i < intHead.numEntries; i++){
                internalBlk.getEntry(&intEntry, i);
                BlockAccess::incrementComparisonCount();
                int compareVal = compareAttrs(intEntry.attrVal, attrVal, attrCatEntry.attrType);
                if((compareVal >= 0 && (op == EQ || op == GE)) || (compareVal > 0 && op == GT)){
                    found = true;
                    break;
                }
            }

            if(found){
                // move to the left child of that block
                block = intEntry.lChild;
            }
            else{
                // move to the right child of the last entry of the block
                block = intEntry.rChild;
            }

        }
    }

    // NOTE: now our block will have the block number of the leaf index block.

    /***
        Identify the first leaf index entry from the current position that satisfies our condition (moving right)
     ***/

     while(block != -1){

        // load the block into leafBlk and get the header
        IndLeaf leafBlk(block);
        HeadInfo leafHead;
        leafBlk.getHeader(&leafHead);

        // declare leafEntry which will be used to store an entry from leafBlk
        Index leafEntry;

        while(index < leafHead.numEntries){
            leafBlk.getEntry(&leafEntry, index);

            BlockAccess::incrementComparisonCount();
            int cmpVal = compareAttrs(leafEntry.attrVal, attrVal, attrCatEntry.attrType);

            if(
                (op == EQ && cmpVal == 0) ||
                (op == LE && cmpVal <= 0) ||
                (op == LT && cmpVal < 0) ||
                (op == GT && cmpVal > 0) ||
                (op == GE && cmpVal >= 0) ||
                (op == NE && cmpVal != 0)
            ){
                // entry satisfying the condition found, set search index
                searchIndex.block = block;
                searchIndex.index = index;
                AttrCacheTable::setSearchIndex(relId, attrName, &searchIndex);

                return RecId{leafEntry.block, leafEntry.slot};

            }
            else if((op == EQ || op == LE || op == LT) && cmpVal > 0){
                /*
                    future entries will not satisfy EQ, LE, LT since the values
                    are arranged in ascending order in the leaves
                */
                return RecId{-1, -1};
            }

            ++index;
        }

        /*
            only for NE operation we have to check the entire linked list.
            For all the other op it is guranteed that the block being searched will have an entry,
            if it exist, satisfying that op
        */
        if(op != NE){
            break;
        }

        block = leafHead.rblock;
        index = 0;
    }

    // no entry satisfying the op was found; return {-1, -1}
    return RecId{-1, -1};
}





