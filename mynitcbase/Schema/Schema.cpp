#include "Schema.h"
#include <cmath>
#include <cstring>

/*
    methods implemented:
        openRel(char relName[ATTR_SIZE])
        closeRel(char relName[ATTR_SIZE])
*/


//opens a relation
int Schema::openRel(char relName[ATTR_SIZE]) {
  
  //returns the rel-id if successful
  int ret = OpenRelTable::openRel(relName);

  // error codes will be negative
  if(ret >= 0){
    return SUCCESS;
  }

  //otherwise it returns an error message
  return ret;
}



//closes a relation
int Schema::closeRel(char relName[ATTR_SIZE]) {

  //we cannot close Relation Catalog or Attribute Catalog
  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) return E_NOTPERMITTED;

  //this function returns the rel-id of a relation if it is open
  int relId = OpenRelTable::getRelId(relName);
  
  //if not open
  if (relId == E_RELNOTOPEN) {
    return E_RELNOTOPEN;
  }

  return OpenRelTable::closeRel(relId);
}



//rename a relation
int Schema::renameRel(char oldRelName[ATTR_SIZE], char newRelName[ATTR_SIZE]) {
  
  //cannot rename Relation Catalog or Attribute Catalog
  if (strcmp(oldRelName, RELCAT_RELNAME) == 0 || strcmp(oldRelName, ATTRCAT_RELNAME) == 0 || strcmp(newRelName, RELCAT_RELNAME) == 0 || strcmp(newRelName, ATTRCAT_RELNAME) == 0 ) return E_NOTPERMITTED;

  //cannot rename if open
  if(OpenRelTable::getRelId(oldRelName) != E_RELNOTOPEN) return E_RELOPEN;
    
  int retVal = BlockAccess::renameRelation(oldRelName, newRelName);
  return retVal;
}



//rename attribute
int Schema::renameAttr(char *relName, char *oldAttrName, char *newAttrName) {

  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) return E_NOTPERMITTED;

  if(OpenRelTable::getRelId(relName) != E_RELNOTOPEN) return E_RELOPEN;
    
  return BlockAccess::renameAttribute(relName, oldAttrName, newAttrName);

}


int Schema::createRel(char relName[],int nAttrs, char attrs[][ATTR_SIZE],int attrtype[]){

  //search if the relation is already present
  Attribute relNameAsAttribute;
  strcpy(relNameAsAttribute.sVal, relName);


  RelCacheTable::resetSearchIndex(RELCAT_RELID);
  RecId targetRelId = BlockAccess::linearSearch(RELCAT_RELID, RELCAT_ATTR_RELNAME, relNameAsAttribute, EQ);


  if(targetRelId.block != -1 && targetRelId.slot != -1) return E_RELEXIST;

  
  //duplicate attributes in the same relation
  for(int i = 0; i < nAttrs; i++){
    for(int j = i + 1; j < nAttrs; j++){
      if(strcmp(attrs[i], attrs[j]) == 0) return E_DUPLICATEATTR;
    }
  }


  //record to be inserted in relation catalog
  Attribute relCatRecord[RELCAT_NO_ATTRS];
  
  strcpy(relCatRecord[RELCAT_REL_NAME_INDEX].sVal,relName);
  relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal = nAttrs;
  relCatRecord[RELCAT_NO_RECORDS_INDEX].nVal = 0;
  relCatRecord[RELCAT_FIRST_BLOCK_INDEX].nVal = -1;
  relCatRecord[RELCAT_LAST_BLOCK_INDEX].nVal = -1;
  int numofslots = floor(2016 * 1.00/ (16 * nAttrs + 1));
  relCatRecord[RELCAT_NO_SLOTS_PER_BLOCK_INDEX].nVal = numofslots;

  
  int retVal = BlockAccess::insert(RELCAT_RELID, relCatRecord);
  if(retVal != SUCCESS) return retVal;


  for(int i = 0; i < nAttrs; i++){

    Attribute attrCatRecord[6];
    strcpy(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relName);
    strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrs[i]);
    attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal = attrtype[i];
    attrCatRecord[ATTRCAT_PRIMARY_FLAG_INDEX].nVal = -1;
    attrCatRecord[ATTRCAT_ROOT_BLOCK_INDEX].nVal = -1;
    attrCatRecord[ATTRCAT_OFFSET_INDEX].nVal = i;

    retVal = BlockAccess::insert(ATTRCAT_RELID, attrCatRecord);
    if(retVal != SUCCESS){
      Schema::deleteRel(relName);
      return E_DISKFULL;
    }
  }

  return SUCCESS;
}


int Schema::deleteRel(char *relName) {
  //not allowed to delete Relation Catalog or Attribute Catalog
  if (strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) return E_NOTPERMITTED;
    
  //get the rel-id
  int relId = OpenRelTable::getRelId(relName);

  //cannot delete if open
  if (relId != E_RELNOTOPEN) {
    return E_RELOPEN;
  }
  
  return BlockAccess::deleteRelation(relName);

}