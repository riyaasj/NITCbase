#include "OpenRelTable.h"

#include <cstring>
#include <stdlib.h>
#include <iostream>

OpenRelTable::OpenRelTable() {
    // initializing relCache and attrCache with null values
    for(int i = 0; i < MAX_OPEN; i++) {
        RelCacheTable::relCache[i] = nullptr;
        AttrCacheTable::attrCache[i] = nullptr;
    }


    /*
        RelCacheEntry - 
            relCatEntry -> header values
            dirty
            recId
            searchIndex

    */

    // populating relation cache
    RecBuffer relCatBuffer(RELCAT_BLOCK);

    //to store the relation catalog record
    Attribute relCatRecord[RELCAT_NO_ATTRS];


    //to find the number or relations we need relation catalog header
    HeadInfo relCatHeader;
    relCatBuffer.getHeader(&relCatHeader);
    


    for(int i = 0; i < relCatHeader.numEntries; i++) {

        //ith relation
        relCatBuffer.getRecord(relCatRecord, i); 
        RelCacheEntry relCacheEntry;

        //convert relation catalog entry into the relCatEntry of relCacheEntry
        RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);

        relCacheEntry.recId.block = RELCAT_BLOCK;
        relCacheEntry.recId.slot = i;

        RelCacheTable::relCache[i] = (RelCacheEntry*)malloc(sizeof(RelCacheEntry));
        *(RelCacheTable::relCache[i]) = relCacheEntry;
    }
    

    /*
        AttrCacheEntry - 
            attrCatEntry -> header values
            dirty
            recId
            searchIndex
            next

    */

    // populating attribute cache
    RecBuffer attrCatBuffer(ATTRCAT_BLOCK);


    HeadInfo attrCatHeader;
    attrCatBuffer.getHeader(&attrCatHeader);

    Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

    AttrCacheEntry* attrCacheEntry = nullptr;
    AttrCacheEntry* head = nullptr;
    AttrCacheEntry* prev = nullptr;

    // Current physical position in the attribute catalog
    int currentBlock = ATTRCAT_BLOCK;
    int currentSlot = 0;


    //for each table in relation catalog
    for (int i = 0; i < relCatHeader.numEntries; i++) {

        //find the number of attributes in the relation from relCache entry
        int noOfAttributes = RelCacheTable::relCache[i]->relCatEntry.numAttrs;


        //for each attribute of the relation
        for (int j = 0; j < noOfAttributes; j++) {

            //get the attribute catalog entry
            attrCatBuffer.getRecord(attrCatRecord, currentSlot);
            attrCacheEntry = (AttrCacheEntry*) malloc(sizeof(AttrCacheEntry));
        
            //convert attribute catalog entry into the attrCatEntry of attrCacheEntry
            AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &(attrCacheEntry->attrCatEntry));


            attrCacheEntry->recId.block = currentBlock;
            attrCacheEntry->recId.slot = currentSlot;
            attrCacheEntry->next = nullptr;


            if (head == nullptr) {
                head = attrCacheEntry;
            }
            else {
                prev->next = attrCacheEntry;
            }

            prev = attrCacheEntry;


            // Move to next attribute-catalog recorr
            if (currentSlot == attrCatHeader.numSlots - 1) {

                // No more blocks
                if (attrCatHeader.rblock == -1) {
                    break;
                }

                // Move to the next attribute catalog block
                currentBlock = attrCatHeader.rblock;
                attrCatBuffer = RecBuffer(currentBlock);

                // Read header of new block
                attrCatBuffer.getHeader(&attrCatHeader);

                // Start from slot 0 of new block
                currentSlot = 0;
            }
            else {
                // Move to next slot in current block
                currentSlot++;
            }
        }

        // Store linked list for this relation
        AttrCacheTable::attrCache[i] = head;

        // Reset pointers for next relation
        head = nullptr;
        prev = nullptr;
    }
}
OpenRelTable::~OpenRelTable() {

    // Free all RelCacheEntry objects
    for (int i = 0; i < MAX_OPEN; i++) {

        if (RelCacheTable::relCache[i] != nullptr) {
            free(RelCacheTable::relCache[i]);
            RelCacheTable::relCache[i] = nullptr;
        }
    }


    // Free all AttrCacheEntry linked lists
    for (int i = 0; i < MAX_OPEN; i++) {

        AttrCacheEntry* current = AttrCacheTable::attrCache[i];

        while (current != nullptr) {
            AttrCacheEntry* next = current->next;
            free(current);
            current = next;
        }

        AttrCacheTable::attrCache[i] = nullptr;
    }
}