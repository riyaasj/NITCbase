#include <iostream>
#include <cstring>
#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"


void printAttributes () {
	
	RecBuffer relCatBuffer(RELCAT_BLOCK);
	RecBuffer attrCatBuffer(ATTRCAT_BLOCK);

	
	HeadInfo relCatHeader;
	HeadInfo attrCatHeader;

	
	relCatBuffer.getHeader(&relCatHeader);
	attrCatBuffer.getHeader(&attrCatHeader);

	
	for (int i = 0, attrCatSlotIndex = 0; i < relCatHeader.numEntries; i++){
		
		Attribute relCatRecord[RELCAT_NO_ATTRS]; 
		relCatBuffer.getRecord(relCatRecord, i);

		printf("Relation: %s\n", relCatRecord[RELCAT_REL_NAME_INDEX].sVal);


		for (int j = 0; j < relCatRecord[RELCAT_NO_ATTRIBUTES_INDEX].nVal; j++, attrCatSlotIndex++){
			
			Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
			attrCatBuffer.getRecord(attrCatRecord, attrCatSlotIndex);

			
			if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relCatRecord[RELCAT_REL_NAME_INDEX].sVal) == 0){
				const char *attrType = attrCatRecord[ATTRCAT_ATTR_TYPE_INDEX].nVal == NUMBER ? "NUM" : "STR";
				printf("  %s: %s\n", attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, attrType);
			}


			if (attrCatSlotIndex == attrCatHeader.numSlots - 1) {
        if(attrCatHeader.rblock == -1) break;
				attrCatSlotIndex = -1; 
				attrCatBuffer = RecBuffer (attrCatHeader.rblock);
				attrCatBuffer.getHeader(&attrCatHeader);
			}
		}

		printf("\n");
	}
}

void updateAttribute(const char* relName, const char* oldAttrName, const char* newAttrName){
  int x = ATTRCAT_BLOCK;
  int f = 0;
  HeadInfo attrCatHeader;
    do{
      RecBuffer attrCatBuffer(x);
      attrCatBuffer.getHeader(&attrCatHeader);

      for (int j = 0; j < attrCatHeader.numEntries; j++) {


        //declare attrCatRecord and load the attribute catalog entry
        Attribute attrCatRecord[ATTRCAT_NO_ATTRS];
        attrCatBuffer.getRecord(attrCatRecord, j);

        //if that att
        if (strcmp(attrCatRecord[ATTRCAT_REL_NAME_INDEX].sVal, relName) == 0 && strcmp(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, oldAttrName) == 0) {
          strcpy(attrCatRecord[ATTRCAT_ATTR_NAME_INDEX].sVal, newAttrName);
          attrCatBuffer.setRecord(attrCatRecord, j);
          f = 1;
          break;
        }
      }
      x = attrCatHeader.rblock;

    } while(x != -1 && f == 0);
}

int main(int argc, char *argv[]) {
  Disk disk_run;

  printAttributes();
  updateAttribute("Sample", "CGPA", "Mark");
  printAttributes();

  return 0;
}