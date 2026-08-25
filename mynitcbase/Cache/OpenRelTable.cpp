#include "OpenRelTable.h"

#include <cstring>
#include <stdlib.h>
#include <iostream>

OpenRelTable::OpenRelTable() {
    /*
        RelCacheEntry - 
            relCatEntry -> header values
            dirty
            recId
            searchIndex

    */
    // initializing relCache and attrCache with null valueds
    for(int i = 0; i < MAX_OPEN; i++) {
        RelCacheTable::relCache[i] = nullptr;
        AttrCacheTable::attrCache[i] = nullptr;
    }

    // populating through relation catalog for the relation cache
    
    RecBuffer relCatBuffer(RELCAT_BLOCK);
    // Attribute relCatRecord[RELCAT_NO_ATTRS];
    Attribute relCatRecord[RELCAT_NO_ATTRS];

    HeadInfo relCatHeader;
    relCatBuffer.getHeader(&relCatHeader);
    
    for(int i = 0; i < relCatHeader.numEntries; i++) {
        relCatBuffer.getRecord(relCatRecord, i); 
        RelCacheEntry relCacheEntry;
        RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
        relCacheEntry.recId.block = RELCAT_BLOCK;
        relCacheEntry.recId.slot = i;

        RelCacheTable::relCache[i] = (RelCacheEntry*)malloc(sizeof(RelCacheEntry));
        *(RelCacheTable::relCache[i]) = relCacheEntry;
    }
    // relCatBuffer.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_RELCAT);

    // RelCacheEntry relCacheEntry;
    // RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
    // relCacheEntry.recId.block = RELCAT_BLOCK;
    // relCacheEntry.recId.slot = RELCAT_SLOTNUM_FOR_RELCAT;

    // RelCacheTable::relCache[RELCAT_RELID] = (RelCacheEntry*)malloc(sizeof(RelCacheEntry));
    // *(RelCacheTable::relCache[RELCAT_RELID]) = relCacheEntry;


    // // setting up Attribute catalog relation in relation catalog table

    // relCatBuffer.getRecord(relCatRecord, RELCAT_SLOTNUM_FOR_ATTRCAT);
    // RelCacheTable::recordToRelCatEntry(relCatRecord, &relCacheEntry.relCatEntry);
    // relCacheEntry.recId.block = RELCAT_BLOCK;
    // relCacheEntry.recId.slot  = RELCAT_SLOTNUM_FOR_ATTRCAT;

    // RelCacheTable::relCache[ATTRCAT_RELID] = (RelCacheEntry*)malloc(sizeof(RelCacheEntry));
    // *(RelCacheTable::relCache[ATTRCAT_RELID]) = relCacheEntry;


    // Setting up the Attribute cache entries //
   RecBuffer attrCatBuffer(ATTRCAT_BLOCK);
Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
HeadInfo attrCatHeader;
attrCatBuffer.getHeader(&attrCatHeader);

AttrCacheEntry* attrCacheEntry = nullptr;
AttrCacheEntry* head = nullptr;
AttrCacheEntry* prev = nullptr;

int attributeCount = 0;
int attrCatSlotIndex = 0;
int attrCatBlock = ATTRCAT_BLOCK;

for(int i = 0; i < relCatHeader.numEntries; i++) {
    // Get the number of attributes for the current relation
    int noOfAttributes = RelCacheTable::relCache[i]->relCatEntry.numAttrs;

    for(int j = 0; j < noOfAttributes; j++) {
        // Get the attribute record from the Attribute Catalog
        attrCatBuffer.getRecord(attrCatRecord, attrCatSlotIndex);

        attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
        AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &(attrCacheEntry->attrCatEntry));

        attrCacheEntry->recId.block = attrCatBlock;
        attrCacheEntry->recId.slot = attrCatSlotIndex;

        if(!head) {
            head = attrCacheEntry;
            prev = attrCacheEntry;
        }

        prev->next = attrCacheEntry;
        prev = attrCacheEntry;

        attrCatSlotIndex++;

        if(attrCatSlotIndex == attrCatHeader.numSlots) {
            attrCatBlock = attrCatHeader.rblock;
            attrCatSlotIndex = 0;

            if(attrCatBlock != -1) {
                attrCatBuffer = RecBuffer(attrCatBlock);
                attrCatBuffer.getHeader(&attrCatHeader);
            }
        }
    }

    prev->next = nullptr;
    AttrCacheTable::attrCache[i] = head;

    attributeCount += noOfAttributes;
    head = nullptr;
    prev = nullptr;
}
    // for(int i = 0; i < RELCAT_NO_ATTRS; i++) {
    //     attrCatBuffer.getRecord(attrCatRecord, i);
    //     attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
    //     AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &(attrCacheEntry->attrCatEntry));
    //     if(!head) {
    //         head = attrCacheEntry;
    //         prev = attrCacheEntry;
    //     }
    //     prev->next = attrCacheEntry;
    //     prev = attrCacheEntry;
    // }
    // attrCacheEntry->next = nullptr;

    // AttrCacheTable::attrCache[RELCAT_RELID] = head;

    // /* Simply printing the linked list for debugging*/

    // // Setting up Attribute Catalog relation in the Attribute Cache Table // 

    // prev = nullptr;
    // head = nullptr;
    // for(int i = 6; i < ATTRCAT_NO_ATTRS + 6; i++) {
    //     attrCatBuffer.getRecord(attrCatRecord, i);
    //     attrCacheEntry = (AttrCacheEntry*)malloc(sizeof(AttrCacheEntry));
    //     AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &(attrCacheEntry->attrCatEntry));
    //     if(!head) {
    //         prev = attrCacheEntry;
    //         head = attrCacheEntry;
    //     }
    //     prev->next = attrCacheEntry;
    //     prev = attrCacheEntry;
    // }

    // attrCacheEntry->next = nullptr;
    // AttrCacheTable::attrCache[ATTRCAT_RELID] = head;
}
void freeLinkedList(AttrCacheEntry** head) {
    if (!head || !*head) return;  
    
    AttrCacheEntry* current = *head;
    while (current) {
        AttrCacheEntry* nextNode = current->next;  
        free(current);  
        current = nextNode; 
    }
    
    *head = nullptr;
}

/* This function will open a relation having name `relName`.
Since we are currently only working with the relation and attribute catalog, we
will just hardcode it. In subsequent stages, we will loop through all the relations
and open the appropriate one.
*/
int OpenRelTable::getRelId(char relName[ATTR_SIZE]) {

  // if relname is RELCAT_RELNAME, return RELCAT_RELID
  // if relname is ATTRCAT_RELNAME, return ATTRCAT_RELID
    if (strcmp(relName, RELCAT_RELNAME) == 0) {
        return RELCAT_RELID;
    } else if (strcmp(relName, ATTRCAT_RELNAME) == 0) {
        return ATTRCAT_RELID;
    }
    else if(strcmp(relName, "Students")==0){
        return ATTRCAT_RELID+1;
    }

  return E_RELNOTOPEN;
}

OpenRelTable::~OpenRelTable() {
    for(int i = RELCAT_RELID; i <= ATTRCAT_RELID; i++) {
        freeLinkedList(&AttrCacheTable::attrCache[i]);
    }
}