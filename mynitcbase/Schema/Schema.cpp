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