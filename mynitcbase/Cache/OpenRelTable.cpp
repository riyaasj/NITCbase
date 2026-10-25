#include "OpenRelTable.h"
#include <cstring>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
using namespace std;

OpenRelTableMetaInfo OpenRelTable::tableMetaInfo[MAX_OPEN];

/*
  methods implemented:
    OpenRelTable()
    ~OpenRelTable()
*/

AttrCacheEntry *createAttrCacheEntryList(int size) {
  AttrCacheEntry *head = nullptr, *curr = nullptr;
  head = curr = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));
  size--;
  while (size--) {
    curr->next = (AttrCacheEntry *)malloc(sizeof(AttrCacheEntry));
    curr = curr->next;
  }
  curr->next = nullptr;
  return head;
}


OpenRelTable::OpenRelTable() {

  // initialize relCache and attrCache with nullptr
  for (int i = 0; i < MAX_OPEN; ++i) {
    RelCacheTable::relCache[i] = nullptr;
    AttrCacheTable::attrCache[i] = nullptr;
    tableMetaInfo[i].free = true;
  }


  // load the relation and attribute catalog into the relation cache
  //we need the relation catalog entries
  RecBuffer relCatBlock(RELCAT_BLOCK);
	Attribute relCatRecord[RELCAT_NO_ATTRS];
	RelCacheEntry *relCacheEntry = nullptr;

  //we only cache the relation catalog entries of the relation catalog and the attribute catalog
  for (int relId = RELCAT_RELID; relId <= ATTRCAT_RELID; relId++) {
    
    relCatBlock.getRecord(relCatRecord, relId);

    relCacheEntry = (RelCacheEntry *)malloc(sizeof(RelCacheEntry));
    RelCacheTable::recordToRelCatEntry(relCatRecord, &(relCacheEntry->relCatEntry));
    
    relCacheEntry->recId.block = RELCAT_BLOCK;
    relCacheEntry->recId.slot = relId;
    relCacheEntry->searchIndex={-1,-1};

    RelCacheTable::relCache[relId] = relCacheEntry;
  }


  // load the relation and attribute catalog into the attribute cache
  RecBuffer attrCatBlock(ATTRCAT_BLOCK);
  Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
  AttrCacheEntry *attrCacheEntry = nullptr, *head = nullptr;

  for (int relId = RELCAT_RELID, recordId = 0; relId <= ATTRCAT_RELID;relId++) {
    int numberOfAttributes = RelCacheTable::relCache[relId]->relCatEntry.numAttrs;
    head = createAttrCacheEntryList(numberOfAttributes);
    attrCacheEntry = head;

    while (numberOfAttributes--) {
      attrCatBlock.getRecord(attrCatRecord, recordId);

      AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &(attrCacheEntry->attrCatEntry));
      attrCacheEntry->recId.slot = recordId++;
      attrCacheEntry->recId.block = ATTRCAT_BLOCK;
      attrCacheEntry = attrCacheEntry->next;
    }

    AttrCacheTable::attrCache[relId] = head;
  }

  //Setting up tableMetaInfo entries
  tableMetaInfo[RELCAT_RELID].free = false;
  tableMetaInfo[ATTRCAT_RELID].free = false;
  strcpy(tableMetaInfo[RELCAT_RELID].relName, "RELATIONCAT");
  strcpy(tableMetaInfo[ATTRCAT_RELID].relName, "ATTRIBUTECAT");
}


OpenRelTable::~OpenRelTable()
{
	// free all the memory that you allocated in the constructor

	//? close all open relations (from rel-id = 2 onwards. Why?)
	for (int i = 2; i < MAX_OPEN; ++i)
		if (!tableMetaInfo[i].free)
			OpenRelTable::closeRel(i); // we will implement this function later

	if(RelCacheTable::relCache[ATTRCAT_RELID]->dirty==true){
		RelCatEntry relCatBuffer;
		RelCacheTable::getRelCatEntry(ATTRCAT_RELID,&relCatBuffer);
		Attribute relCatRecord[RELCAT_NO_ATTRS];
		RelCacheTable::relCatEntryToRecord(&relCatBuffer,relCatRecord);
		RecId recId=RelCacheTable::relCache[ATTRCAT_RELID]->recId;
		RecBuffer relCatBlock(recId.block);
		relCatBlock.setRecord(relCatRecord,recId.slot);
	}
	free(RelCacheTable::relCache[ATTRCAT_RELID]);

	if(RelCacheTable::relCache[RELCAT_RELID]->dirty==true){
		RelCatEntry relCatBuffer;
		RelCacheTable::getRelCatEntry(RELCAT_RELID,&relCatBuffer);
		Attribute relCatRecord[ATTRCAT_NO_ATTRS];
		RelCacheTable::relCatEntryToRecord(&relCatBuffer,relCatRecord);
		RecId recId=RelCacheTable::relCache[RELCAT_RELID]->recId;
		RecBuffer attrCatRecord(recId.block);
		attrCatRecord.setRecord(relCatRecord,recId.slot);

	}
	free(RelCacheTable::relCache[RELCAT_RELID]);
	// free the memory allocated for rel-id 0 and 1 in the caches
	for(int relID=RELCAT_RELID;relID<=ATTRCAT_RELID;relID++){
		AttrCacheEntry *curr=AttrCacheTable::attrCache[relID],*next=NULL;
		while(curr!=nullptr){
			next=curr->next;
			if(curr->dirty==true){
				AttrCatEntry attrCatEntry=curr->attrCatEntry;
				Attribute AttrCatrecord[ATTRCAT_NO_ATTRS];
				AttrCacheTable::attrCatEntryToRecord(&attrCatEntry,AttrCatrecord);
				RecBuffer attrCatBlock(curr->recId.block);
				attrCatBlock.setRecord(AttrCatrecord,curr->recId.slot);
			}
			free(curr);
			curr=next;
		}
	}
}

int OpenRelTable::getRelId(char relName[ATTR_SIZE]) {
  for (int i = 0; i < MAX_OPEN; i++) {
    if (strcmp(relName, tableMetaInfo[i].relName) == 0 and tableMetaInfo[i].free == false) {
      return i;
    }
  }
  return E_RELNOTOPEN;
}



int OpenRelTable::getFreeOpenRelTableEntry() {

  for (int i = 0; i < MAX_OPEN; i++) {
    if (tableMetaInfo[i].free) {
      return i;
    }
  }
  return E_CACHEFULL;
}


int OpenRelTable::openRel(char relName[ATTR_SIZE]) {

	//check if already in open rel table
  int relId = getRelId(relName);
  if(relId >= 0){
    return relId;
  }

  //find free slot in open rel table
  relId = OpenRelTable::getFreeOpenRelTableEntry();

  //if cache is full
  if (relId == E_CACHEFULL) return E_CACHEFULL;
  
  //do linear search in relation catalog with relation name
  Attribute attrVal; 
  strcpy(attrVal.sVal, relName);

  //reset search index of relation catalog
  RelCacheTable::resetSearchIndex(RELCAT_RELID);

  RecId relcatRecId = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, attrVal, EQ);

  //the relation is not found in the Relation Catalog
  if (relcatRecId.block == -1 and relcatRecId.slot == -1) {
    return E_RELNOTEXIST;
  }


	//get the record and convert it into relCatEntry of a relCacheEntry
	RecBuffer relationBuffer (relcatRecId.block);

	Attribute relationRecord [RELCAT_NO_ATTRS];
  relationBuffer.getRecord(relationRecord, relcatRecId.slot);

	RelCacheEntry *relCacheBuffer = (RelCacheEntry*) malloc (sizeof(RelCacheEntry));
	RelCacheTable::recordToRelCatEntry(relationRecord, &(relCacheBuffer->relCatEntry));

	//finish updating relCacheEntry attributes
	relCacheBuffer->recId.block = relcatRecId.block;
	relCacheBuffer->recId.slot = relcatRecId.slot;
	
	//add the entry into relation cache
	RelCacheTable::relCache[relId] = relCacheBuffer;	




	Attribute attrCatRecord[ATTRCAT_NO_ATTRS];

	//make list for attr cache entry
	AttrCacheEntry *attrCacheEntry = nullptr, *head = nullptr;
	int numberOfAttributes = RelCacheTable::relCache[relId]->relCatEntry.numAttrs;
	head = createAttrCacheEntryList(numberOfAttributes);
	attrCacheEntry = head;


	RelCacheTable::resetSearchIndex(ATTRCAT_RELID);

	for (int attr = 0; attr < numberOfAttributes; attr++){
    
    //find the entry in attribute cache with same relation name
		RecId attrcatRecId = BlockAccess::linearSearch(ATTRCAT_RELID, RELCAT_ATTR_RELNAME, attrVal, EQ);

		RecBuffer attrCatBlock(attrcatRecId.block);
		attrCatBlock.getRecord(attrCatRecord, attrcatRecId.slot);

		AttrCacheTable::recordToAttrCatEntry(attrCatRecord, &(attrCacheEntry->attrCatEntry));

		attrCacheEntry->recId.block = attrcatRecId.block;
		attrCacheEntry->recId.slot = attrcatRecId.slot;

		attrCacheEntry = attrCacheEntry->next;
	}


	AttrCacheTable::attrCache[relId] = head;
	tableMetaInfo[relId].free = false;
	strcpy(tableMetaInfo[relId].relName, relName);

  return relId;
}




int OpenRelTable::closeRel(int relId) {

  //cannot close
  if (relId == 0 or relId == 1) {
    return E_NOTPERMITTED;
  }

  //invalid relId
  if (relId < 0 || relId >= MAX_OPEN) {
    return E_OUTOFBOUND;
  }

  //not open
  if (tableMetaInfo[relId].free) return E_RELNOTOPEN;

  if (AttrCacheTable::attrCache[relId] == nullptr) {
    return E_RELNOTOPEN;
  }

  
  //if the relation cache entry is dirty, write it back to the buffer
  if(RelCacheTable::relCache[relId]->dirty == true){

    //get the block and slot of the relation catalog entry 
    Attribute relCatBuffer[RELCAT_NO_ATTRS];
		RelCacheTable::relCatEntryToRecord(&(RelCacheTable::relCache[relId]->relCatEntry), relCatBuffer);
		RecId recId = RelCacheTable::relCache[relId]->recId;
		RecBuffer relCatBlock(recId.block);

    //write back the realtion catalog entry
		relCatBlock.setRecord(relCatBuffer, RelCacheTable::relCache[relId]->recId.slot);
  }


  //free relCacheTable entry
  free(RelCacheTable::relCache[relId]);



  //free attrCacheTableEntry
  AttrCacheEntry *head = AttrCacheTable::attrCache[relId];
  while(head){

    if(head->dirty == true){
      Attribute attrCatBuffer[ATTRCAT_NO_ATTRS];
      AttrCacheTable::attrCatEntryToRecord(&(head->attrCatEntry), attrCatBuffer);
      RecId recId = head->recId;

      RecBuffer attrCatBlock(recId.block);
      attrCatBlock.setRecord(attrCatBuffer, recId.slot);
    }
    AttrCacheEntry *temp = head;
    head = head->next;
    free(temp);

  }

  // update `tableMetaInfo` to set `relId` as a free slot
  tableMetaInfo[relId].free = true;

  // update `relCache` and `attrCache` to set the entry at `relId` to nullptr

  AttrCacheTable::attrCache[relId] = nullptr;
  RelCacheTable::relCache[relId] = nullptr;

  return SUCCESS;
}
