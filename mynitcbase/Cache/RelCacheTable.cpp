#include "RelCacheTable.h"
#include <cstring>

RelCacheEntry* RelCacheTable::relCache[MAX_OPEN];

/*
  methods implemented:
    getRelCatEntry(int relId, RelCatEntry* relCatBuf)
    recordToRelCatEntry(union Attribute record[RELCAT_NO_ATTRS], RelCatEntry* relCatEntry)
    getSearchIndex(int relId, RecId *searchIndex)
    setSearchIndex(int relId, RecId *searchIndex)
    resetSearchIndex(int relId)
*/


//returns the relCatEntry inside the relCacheEnty
int RelCacheTable::getRelCatEntry(int relId, RelCatEntry* relCatBuf) {

  //check if relId valid
  if (relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  // if there's no entry at the rel-id
  if (relCache[relId] == nullptr) {
    return E_RELNOTOPEN;
  }

  // copy the value to the relCatBuf argument
  *relCatBuf = relCache[relId]->relCatEntry;

  return SUCCESS;
}


//Converts a relation catalog record to RelCatEntry struct
void RelCacheTable::recordToRelCatEntry(union Attribute record[RELCAT_NO_ATTRS], RelCatEntry* relCatEntry) {
  strcpy(relCatEntry->relName, record[RELCAT_REL_NAME_INDEX].sVal);
  relCatEntry->numAttrs = (int)record[RELCAT_NO_ATTRIBUTES_INDEX].nVal;
  relCatEntry->numRecs = (int)record[RELCAT_NO_RECORDS_INDEX].nVal;
  relCatEntry->firstBlk = (int)record[RELCAT_FIRST_BLOCK_INDEX].nVal;
  relCatEntry->lastBlk = (int)record[RELCAT_LAST_BLOCK_INDEX].nVal;
  relCatEntry->numSlotsPerBlk = (int)record[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal;

}


int RelCacheTable::getSearchIndex(int relId, RecId *searchIndex) {

  //check if relId valid
  if (relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  // if there's no entry at the rel-id
  if (relCache[relId] == nullptr) {
    return E_RELNOTOPEN;
  }

  *searchIndex = relCache[relId]->searchIndex;
  return SUCCESS;
}


int RelCacheTable::setSearchIndex(int relId, RecId *searchIndex) {

  if (relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  if (relCache[relId] == nullptr) {
    return E_RELNOTOPEN;
  }

  relCache[relId]->searchIndex = *searchIndex;
  return SUCCESS;
}


int RelCacheTable::resetSearchIndex(int relId) {
  if (relId < 0 || relId >= MAX_OPEN)
		return E_OUTOFBOUND;

	if (RelCacheTable::relCache[relId] == nullptr)
		return E_RELNOTOPEN;
    
  RelCacheTable::relCache[relId]->searchIndex = {-1, -1};
  return SUCCESS;
}



int RelCacheTable::setRelCatEntry(int relId, RelCatEntry *relCatBuf) {

  //relId invalid
  if (relId < 0 || relId >= MAX_OPEN)
		return E_OUTOFBOUND;


  //relation not open
	if (RelCacheTable::relCache[relId] == nullptr)
		return E_RELNOTOPEN;


  // copy the relCatBuf to the corresponding Relation Catalog entry in the Relation Cache Table.
  memcpy(&(RelCacheTable::relCache[relId]->relCatEntry), relCatBuf, sizeof(RelCatEntry));

  // set the dirty flag of the corresponding Relation Cache entry in the Relation Cache Table.
  RelCacheTable::relCache[relId]->dirty = true;

  return SUCCESS;
}


void RelCacheTable::relCatEntryToRecord(RelCatEntry *relCatEntry, union Attribute record[RELCAT_NO_ATTRS]){
  strcpy(record[RELCAT_REL_NAME_INDEX].sVal,relCatEntry->relName);
  record[RELCAT_NO_ATTRIBUTES_INDEX].nVal=relCatEntry->numAttrs;
  record[RELCAT_NO_RECORDS_INDEX].nVal=relCatEntry->numRecs;
  record[RELCAT_FIRST_BLOCK_INDEX].nVal=relCatEntry->firstBlk;
  record[RELCAT_LAST_BLOCK_INDEX].nVal=relCatEntry->lastBlk;
  record[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal=relCatEntry->numSlotsPerBlk;
}