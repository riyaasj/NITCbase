#include "AttrCacheTable.h"
#include <cstring>

AttrCacheEntry* AttrCacheTable::attrCache[MAX_OPEN];

/*
  methods implemented:
    getAttrCatEntry(int relId, int attrOffset, AttrCatEntry* attrCatBuf)
    recordToAttrCatEntry(union Attribute record[ATTRCAT_NO_ATTRS], AttrCatEntry* attrCatEntry)
    getAttrCatEntry(int relId, char attrName[ATTR_SIZE], AttrCatEntry* attrCatBuf)
*/


//returns the attCatEntry inside the attrCacheEnty
int AttrCacheTable::getAttrCatEntry(int relId, int attrOffset, AttrCatEntry* attrCatBuf) {
  
  //check if relId valid
  if (relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  //check if not open
  if (attrCache[relId] == nullptr) {
    return E_RELNOTOPEN;
  }

  // traverse the linked list of attribute cache entries
  for (AttrCacheEntry* entry = attrCache[relId]; entry != nullptr; entry = entry->next) {
    if (entry->attrCatEntry.offset == attrOffset) {
      *attrCatBuf = entry->attrCatEntry;
      return SUCCESS;
    }
  }

  //there is no attribute at this offset
  return E_ATTRNOTEXIST;
}


//Converts an attribute catalog record to AttrCatEntry struct
void AttrCacheTable::recordToAttrCatEntry(union Attribute record[ATTRCAT_NO_ATTRS], AttrCatEntry* attrCatEntry) {

  strcpy(attrCatEntry->relName, record[ATTRCAT_REL_NAME_INDEX].sVal);
  strcpy(attrCatEntry->attrName, record[ATTRCAT_ATTR_NAME_INDEX].sVal);
  attrCatEntry->attrType = (int)record[ATTRCAT_ATTR_TYPE_INDEX].nVal;
  attrCatEntry->primaryFlag = (int)record[ATTRCAT_PRIMARY_FLAG_INDEX].nVal;
  attrCatEntry->rootBlock = (int)record[ATTRCAT_ROOT_BLOCK_INDEX].nVal;
  attrCatEntry->offset = (int)record[ATTRCAT_OFFSET_INDEX].nVal;

}


//with attrname
int AttrCacheTable::getAttrCatEntry(int relId, char attrName[ATTR_SIZE], AttrCatEntry* attrCatBuf) {

  // check that relId is valid and corresponds to an open relation
  if (relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  if (attrCache[relId] == nullptr) {
    return E_RELNOTOPEN;
  }

  //iterate attribute cache to find entry in that relId with that attr name
  for (AttrCacheEntry* entry = attrCache[relId]; entry != nullptr; entry = entry->next) {
    if (strcmp(attrName, entry->attrCatEntry.attrName) == 0) {
      *attrCatBuf = entry->attrCatEntry;
      return SUCCESS;
    }
  }

  return E_ATTRNOTEXIST;
}

void AttrCacheTable::attrCatEntryToRecord(AttrCatEntry *attrCatEntry, Attribute record[ATTRCAT_NO_ATTRS]){
    strcpy(record[ATTRCAT_REL_NAME_INDEX].sVal, attrCatEntry->relName);
    strcpy(record[ATTRCAT_ATTR_NAME_INDEX].sVal, attrCatEntry->attrName);

    record[ATTRCAT_ATTR_TYPE_INDEX].nVal = attrCatEntry->attrType;
    record[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = attrCatEntry->primaryFlag;
    record[ATTRCAT_ROOT_BLOCK_INDEX].nVal = attrCatEntry->rootBlock;
    record[ATTRCAT_OFFSET_INDEX].nVal = attrCatEntry->offset;

}



int AttrCacheTable::getSearchIndex(int relId, char attrName[ATTR_SIZE], IndexId *searchIndex){
    if(relId < 0 || relId >= MAX_OPEN){
        return E_OUTOFBOUND;
    }

    if(attrCache[relId] == nullptr){
        return E_RELNOTOPEN;
    }

    for(AttrCacheEntry *entry = attrCache[relId]; entry != nullptr; entry = entry->next){
        if(strcmp(attrName,entry->attrCatEntry.attrName) == 0){
            *searchIndex = entry->searchIndex;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

int AttrCacheTable::getSearchIndex(int relId, int attrOffset, IndexId *searchIndex ){
    if(relId < 0 || relId >= MAX_OPEN){
        return E_OUTOFBOUND;
    }

    if(attrCache[relId] == nullptr){
        return E_RELNOTOPEN;
    }

    for(AttrCacheEntry *entry = attrCache[relId]; entry != nullptr; entry = entry->next){
        if(entry->attrCatEntry.offset == attrOffset){
            *searchIndex = entry->searchIndex;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}



int AttrCacheTable::setSearchIndex(int relId, char attrName[ATTR_SIZE], IndexId *searchIndex){
    if(relId < 0 || relId >= MAX_OPEN){
        return E_OUTOFBOUND;
    }

    if(attrCache[relId] == nullptr){
        return E_RELNOTOPEN;
    }

    for(AttrCacheEntry *entry = attrCache[relId]; entry != nullptr; entry = entry->next){
        if(strcmp(attrName,entry->attrCatEntry.attrName) == 0){
            entry->searchIndex = *searchIndex;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}

int AttrCacheTable::setSearchIndex(int relId, int attrOffset, IndexId *searchIndex){
    if(relId < 0 || relId >= MAX_OPEN){
        return E_OUTOFBOUND;
    }

    if(attrCache[relId] == nullptr){
        return E_RELNOTOPEN;
    }

    for(AttrCacheEntry *entry = attrCache[relId]; entry != nullptr; entry = entry->next){
        if(entry->attrCatEntry.offset == attrOffset){
            entry->searchIndex = *searchIndex;
            return SUCCESS;
        }
    }

    return E_ATTRNOTEXIST;
}


int AttrCacheTable::resetSearchIndex(int relId, char attrName[ATTR_SIZE]){
    IndexId searchIndex = {-1, -1};
    return AttrCacheTable::setSearchIndex(relId, attrName, &searchIndex);
}

int AttrCacheTable::resetSearchIndex(int relId, int attrOffset){
    IndexId searchIndex = {-1, -1};
    return AttrCacheTable::setSearchIndex(relId, attrOffset, &searchIndex);
}