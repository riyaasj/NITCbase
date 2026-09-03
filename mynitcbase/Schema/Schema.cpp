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
