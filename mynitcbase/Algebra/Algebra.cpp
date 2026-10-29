#include "Algebra.h"
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iostream>
using namespace std;


bool isNumber(char *str) {
  char *end;
  std::strtof(str, &end);
  if (end == str) {
    return false;
  }

  while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end))) {
    end++;
  }
  return *end == '\0';
}



int Algebra::insert(char relName[ATTR_SIZE], int nAttrs, char record[][ATTR_SIZE]){
    
  //not allowed to insert into relation catalog/ attribute catalog
  if(strcmp(relName, RELCAT_RELNAME) == 0 || strcmp(relName, ATTRCAT_RELNAME) == 0) return E_NOTPERMITTED;

  // get the relation's rel-id 
  int relId = OpenRelTable::getRelId(relName);

  // if relation is not open
  if(relId == E_RELNOTOPEN) return E_RELNOTOPEN;


  // get the relation catalog entry from relation cache
  RelCatEntry relCatEntry;
  RelCacheTable::getRelCatEntry(relId, &relCatEntry);


  //mismatch in number of attributes
  if (relCatEntry.numAttrs != nAttrs) return E_NATTRMISMATCH;

  Attribute recordValues[nAttrs];

  //Converting 2D char array of record values to Attribute array recordValues
  for(int i = 0; i < nAttrs; i++){

    // get the attr-cat entry for the i'th attribute from the attr-cache
    AttrCatEntry attrCatEntry;
    AttrCacheTable::getAttrCatEntry(relId, i, &attrCatEntry);
    
    int type = attrCatEntry.attrType;

    if (type == NUMBER){
        if(isNumber(record[i])){
            recordValues[i].nVal = atof(record[i]);
        }
        else{
          return E_ATTRTYPEMISMATCH;
        }
    }

    else if (type == STRING){
      strcpy(recordValues[i].sVal,record[i]);
    }
  }

  
  int retVal = BlockAccess::insert(relId, recordValues);
  return retVal;
}


int Algebra::select(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE], char attr[ATTR_SIZE], int op, char strVal[ATTR_SIZE]){
  
  //check if the relation is open
  int srcRelId = OpenRelTable::getRelId(srcRel);
  if(srcRelId == E_RELNOTOPEN){
      return E_RELNOTOPEN;
  }


  //convert strVal into attribute
  AttrCatEntry attrCatEntry;
  int ret = AttrCacheTable::getAttrCatEntry(srcRelId, attr, &attrCatEntry);
  if (ret == E_ATTRNOTEXIST) {
    return E_ATTRNOTEXIST;
  }
    
  
  int type = attrCatEntry.attrType;
  Attribute attrValue;
  if(type == NUMBER){
      if(isNumber(strVal)){
          attrValue.nVal = atof(strVal);
      }
      else{
          return E_ATTRTYPEMISMATCH;
      }
  }
  else if( type == STRING){
      strcpy(attrValue.sVal, strVal);
  }


  //creating target relation
  RelCatEntry relCatEntry;
  RelCacheTable::getRelCatEntry(srcRelId, &relCatEntry);
  int src_nAttrs = relCatEntry.numAttrs;


  char attr_names[src_nAttrs][ATTR_SIZE];
  int attr_type[src_nAttrs];

  for(int i = 0; i < src_nAttrs; i++){
      AttrCatEntry srcAttrCatEntry;
      AttrCacheTable::getAttrCatEntry(srcRelId, i, &srcAttrCatEntry);
      strcpy(attr_names[i],srcAttrCatEntry.attrName);
      attr_type[i] = srcAttrCatEntry.attrType;
  }

  //creating target relation
  int retVal = Schema::createRel(targetRel, src_nAttrs, attr_names, attr_type);


  if(retVal != SUCCESS){
      Schema::deleteRel(targetRel);
      return retVal;
  }

  //open the new relation
  int targetRelId = OpenRelTable::openRel(targetRel);
  if(targetRelId < 0) {
      return targetRelId;
  }


  //insert into the target relation
  Attribute record[src_nAttrs];
  RelCacheTable::resetSearchIndex(srcRelId);
  AttrCacheTable::resetSearchIndex(srcRelId,attr);

  BlockAccess::resetComparisonCount();

  while(BlockAccess::search(srcRelId, record, attr, attrValue, op) == SUCCESS){
      int ret = BlockAccess::insert(targetRelId, record);
      if(ret != SUCCESS){
          Schema::closeRel(targetRel);
          Schema::deleteRel(targetRel);
          return ret;
      }
  }
  cout << "Number of comparisons: " << BlockAccess::getComparisonCount() << endl;
  Schema::closeRel(targetRel);
  return SUCCESS;
}



int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE]){

    //check if table is open
    int srcRelId = OpenRelTable::getRelId(srcRel);
    if(srcRelId < 0){
        return srcRelId;
    }

    //get the info of the source table to make the target table 
    RelCatEntry srcRelCatEntry;
    RelCacheTable::getRelCatEntry(srcRelId, &srcRelCatEntry);

    int numAttrs = srcRelCatEntry.numAttrs;
    
    char attrNames[numAttrs][ATTR_SIZE];
    int attrTypes[numAttrs];


    //iterate through every attribute of the source relation and fill the arrays attrNames, attrTypes
    for(int i = 0; i < numAttrs; i++){
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(srcRelId, i, &attrCatEntry);
        strcpy(attrNames[i], attrCatEntry.attrName);
        attrTypes[i] = attrCatEntry.attrType;
    }


    //create target relation
    int ret = Schema::createRel(targetRel,numAttrs,attrNames, attrTypes);
    if(ret != SUCCESS){
        return ret;
    }

    //open target relation
    int targetRelId = OpenRelTable::openRel(targetRel);
    if(targetRelId < 0){
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    //insert the projected records into new table
    RelCacheTable::resetSearchIndex(srcRelId);
    Attribute record[numAttrs];

    while(BlockAccess::project(srcRelId, record) == SUCCESS){
        ret = BlockAccess::insert(targetRelId, record);
        if(ret != SUCCESS){
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }

    Schema::closeRel(targetRel);
    
    return SUCCESS;
}

/*
    project()
*/
int Algebra::project(char srcRel[ATTR_SIZE], char targetRel[ATTR_SIZE],int tar_nAttrs, char tar_Attrs[][ATTR_SIZE]){
    int srcRelId = OpenRelTable::getRelId(srcRel);
    if(srcRelId < 0){
        return srcRelId;
    }

    RelCatEntry srcRelCatEntry;
    RelCacheTable::getRelCatEntry(srcRelId, &srcRelCatEntry);
    int numAttrs = srcRelCatEntry.numAttrs;

    // create attr_offset[tar_nAttrs] of type int
    // create attr_types[tar_nAttrs] of type int

    int attr_offset[tar_nAttrs];
    int attr_types[tar_nAttrs];

    /*** Checking if attributes of target are peresent in the source relation and stores its offset and types ***/
    for(int i = 0; i < tar_nAttrs; i++){
        AttrCatEntry attrCatEntry;
        int ret = AttrCacheTable::getAttrCatEntry(srcRelId, tar_Attrs[i], &attrCatEntry);
        if(ret != SUCCESS){
            return ret;
        }

        attr_offset[i] = attrCatEntry.offset;
        attr_types[i] = attrCatEntry.attrType;
    }

    /*** Creating and opening the target relation ***/
    // create
    int ret = Schema::createRel(targetRel, tar_nAttrs, tar_Attrs, attr_types);
    if(ret != SUCCESS){
        return ret;
    }

    // open
    int targetRelId = OpenRelTable::openRel(targetRel);
    if(targetRelId < 0){
        Schema::deleteRel(targetRel);
        return targetRelId;
    }

    /*** Inserting projected records into the target relation ***/
    RelCacheTable::resetSearchIndex(srcRelId);
    Attribute record[numAttrs];
    
    while(BlockAccess::project(srcRelId, record) == SUCCESS){
        Attribute proj_record[tar_nAttrs];

        for(int i = 0; i < tar_nAttrs; i++){
            proj_record[i] = record[attr_offset[i]];
        }

        // insert the record
        ret = BlockAccess::insert(targetRelId, proj_record);
        if(ret != SUCCESS){
            Schema::closeRel(targetRel);
            Schema::deleteRel(targetRel);
            return ret;
        }
    }

    // close target relation
    Schema::closeRel(targetRel);

    return SUCCESS;

}


int Algebra::join(char srcRelation1[ATTR_SIZE], char srcRelation2[ATTR_SIZE], char targetRelation[ATTR_SIZE], char attribute1[ATTR_SIZE], char attribute2[ATTR_SIZE]) {

    //check if both the source relations are open
    int srcRelId1 = OpenRelTable::getRelId(srcRelation1);
    int srcRelId2 = OpenRelTable::getRelId(srcRelation2);

    if(srcRelId1 < 0 || srcRelId2 < 0){
        return E_RELNOTOPEN;
    }

    //get the attribute catalof entries
    AttrCatEntry attrCatEntry1, attrCatEntry2;
    int ret = AttrCacheTable::getAttrCatEntry(srcRelId1, attribute1, &attrCatEntry1);
    if(ret != SUCCESS){
        return ret;
    }

    ret = AttrCacheTable::getAttrCatEntry(srcRelId2, attribute2, &attrCatEntry2);
    if(ret != SUCCESS){
        return ret;
    }


    //attributes are of different types
    if(attrCatEntry1.attrType != attrCatEntry2.attrType){
        return E_ATTRTYPEMISMATCH;
    }



    RelCatEntry relCatEntry1, relCatEntry2;
    RelCacheTable::getRelCatEntry(srcRelId1, &relCatEntry1);
    RelCacheTable::getRelCatEntry(srcRelId2, &relCatEntry2);


    //find if there are any othe common attributes in the two relations other than the join attributes
    for(int i = 0; i < relCatEntry1.numAttrs; i++){

        if(i == attrCatEntry1.offset) continue;

        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(srcRelId1, i, &attrCatEntry);

        for(int j = 0; j < relCatEntry2.numAttrs; j++){

            if(j == attrCatEntry2.offset) continue;
            AttrCatEntry attrCatEntry2;
            AttrCacheTable::getAttrCatEntry(srcRelId2, j, &attrCatEntry2);
            if(strcmp(attrCatEntry.attrName, attrCatEntry2.attrName) == 0){
                return E_DUPLICATEATTR;
            }
        }
    }



    int numOfAttributes1 = relCatEntry1.numAttrs;
    int numOfAttributes2 = relCatEntry2.numAttrs;

   
    //create the B+ tree index for attribute2 in srcRelation2 if it doesn't exist 
    if(attrCatEntry2.rootBlock == -1){
        ret = BPlusTree::bPlusCreate(srcRelId2, attribute2);
        if(ret != SUCCESS){
            return ret;
        }
    }


    //creating target realtion
    int numOfAttributesInTarget = numOfAttributes1 + numOfAttributes2 - 1;


    char targetRelAttrNames[numOfAttributesInTarget][ATTR_SIZE];
    int targetRelAttrTypes[numOfAttributesInTarget];


    for(int i = 0; i < numOfAttributes1; i++){
        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(srcRelId1, i, &attrCatEntry);
        strcpy(targetRelAttrNames[i], attrCatEntry.attrName);
        targetRelAttrTypes[i] = attrCatEntry.attrType;
    }
    int targetAttrOffset = numOfAttributes1;
    for(int i = 0; i < numOfAttributes2; i++){
        //common attribute, skip it
        if(i == attrCatEntry2.offset) continue;

        AttrCatEntry attrCatEntry;
        AttrCacheTable::getAttrCatEntry(srcRelId2, i, &attrCatEntry);
        strcpy(targetRelAttrNames[targetAttrOffset], attrCatEntry.attrName);
        targetRelAttrTypes[targetAttrOffset] = attrCatEntry.attrType;
        targetAttrOffset++;
    }


    //create the target relation
    ret = Schema::createRel(targetRelation, numOfAttributesInTarget, targetRelAttrNames, targetRelAttrTypes);

    if(ret != SUCCESS) {
        return ret;
    }

    //Open the targetRelation
    int targetRelId = OpenRelTable::openRel(targetRelation);

    if(targetRelId < 0){
        Schema::deleteRel(targetRelation);
        return targetRelId;
    }

    Attribute record1[numOfAttributes1];
    Attribute record2[numOfAttributes2];
    Attribute targetRecord[numOfAttributesInTarget];

    // this loop is to get every record of the srcRelation1 one by one
    ret = RelCacheTable::resetSearchIndex(srcRelId1);
    if(ret != SUCCESS) {
        OpenRelTable::closeRel(targetRelId);
        Schema::deleteRel(targetRelation);
        return ret;
    }

    while (BlockAccess::project(srcRelId1, record1) == SUCCESS) {


        RelCacheTable::resetSearchIndex(srcRelId2);
        AttrCacheTable::resetSearchIndex(srcRelId2, attribute2);

        //find the records in srcRelation2 which have the same value for attribute2
        while (BlockAccess::search(srcRelId2, record2, attribute2, record1[attrCatEntry1.offset], EQ) == SUCCESS ){

            //copy into targetRecord
            for(int i = 0; i < numOfAttributes1; i++){
                targetRecord[i] = record1[i];
            }
            targetAttrOffset = numOfAttributes1;
            for(int i = 0; i < numOfAttributes2; i++){
                if(i == attrCatEntry2.offset) continue;
                targetRecord[targetAttrOffset] = record2[i];
                targetAttrOffset++;
            }

            // insert the current record into the target relation by calling
            // BlockAccess::insert()
            int ret = BlockAccess::insert(targetRelId, targetRecord);

            if(ret != SUCCESS) {
                OpenRelTable::closeRel(targetRelId);
                Schema::deleteRel(targetRelation);
                return E_DISKFULL;
            }
        }
    }

    // close the target relation by calling OpenRelTable::closeRel()
    OpenRelTable::closeRel(targetRelId);
    return SUCCESS;
}