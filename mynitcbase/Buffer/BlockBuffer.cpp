#include "BlockBuffer.h"
#include <cstdlib>
#include <cstring>

/*
  methods implemented:
    BlockBuffer:
      BlockBuffer(int blockNum);
      getHeader(struct HeadInfo *head);
      loadBlockAndGetBufferPtr(unsigned char **buffPtr)
    
    RecBuffer:
      RecBuffer(int blockNum);
      getRecord(union Attribute *rec, int slotNum)
      getSlotMap(unsigned char *slotMap)
*/



//BlockBuffer constructor
BlockBuffer::BlockBuffer(int blockNum) {
  this->blockNum = blockNum;
}



// load the block header into the argument pointer
int BlockBuffer::getHeader(struct HeadInfo *head) {

  //create a pointer to buffer where the block is loaded
  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) {
    return ret;   
  }

  //populate the header entries into *head
  memcpy(&head->numSlots, bufferPtr + 24, 4);
  memcpy(&head->numEntries, bufferPtr + 16, 4);
  memcpy(&head->numAttrs, bufferPtr + 20, 4);
  memcpy(&head->rblock, bufferPtr + 12, 4);
  memcpy(&head->lblock, bufferPtr + 8, 4);

  return SUCCESS;
}



//Used to load a block to the buffer and get a pointer to it.
int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr) {

  // check whether the block is already present in the buffer
  int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

  //if not, get a free buffer block and load from disk
  if (bufferNum == E_BLOCKNOTINBUFFER) {
    bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

    if (bufferNum == E_OUTOFBOUND) {
      return E_OUTOFBOUND;
    }

    Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
  }
  else{
    for(int i = 0; i < BUFFER_CAPACITY; i++){
      if(i == bufferNum){
        StaticBuffer::metainfo[i].timeStamp = 0;
      }
      else if(StaticBuffer::metainfo[i].free == false){
        StaticBuffer::metainfo[i].timeStamp++;
      }
    }
  }

  // store the pointer to this buffer
  *buffPtr = StaticBuffer::blocks[bufferNum];

  return SUCCESS;
}



//RecBuffer constructor, : introduces the constructor initializer list; before executing the body of this constructor initialize base class using this
RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {}



//load the record at slotNum into the argument pointer
int RecBuffer::getRecord(union Attribute *rec, int slotNum) {

  //create a pointer to buffer where the block is loaded
  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) {
    return ret;   
  }

  //we need header to get number of attributes and number of slots
  struct HeadInfo head;
  this->getHeader(&head);

  int attrCount = head.numAttrs;
  int slotCount = head.numSlots;

  int recordSize = attrCount * ATTR_SIZE;
  unsigned char *slotPointer = bufferPtr + HEADER_SIZE + slotCount + (recordSize * slotNum);

  // load the record into the rec
  memcpy(rec, slotPointer, recordSize);

  return SUCCESS;
}


int RecBuffer::getSlotMap(unsigned char *slotMap) {

  unsigned char *bufferPtr;
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);

  if (ret != SUCCESS) {
    return ret;
  }

  //get the header of the block using getHeader() function
  struct HeadInfo head;
  BlockBuffer::getHeader(&head);

  int slotCount = head.numSlots; //number of slots in block from header

  // get a pointer to the beginning of the slotmap in memory by offsetting HEADER_SIZE
  unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;

  memcpy(slotMap, slotMapInBuffer, slotCount);

  return SUCCESS;
}


int compareAttrs(union Attribute attr1, union Attribute attr2, int attrType) {

    double diff;
    if(attrType == STRING){
      diff = strcmp(attr1.sVal, attr2.sVal);
    }
    else{
      diff = attr1.nVal - attr2.nVal;
    }

    if(diff > 0) return 1;
    if(diff < 0) return -1;
    return 0;
   
}


int RecBuffer::setRecord(union Attribute *rec, int slotNum) {
  
  //get the buffer block containing the block
  unsigned char *bufferPtr;
  int bufferNum=BlockBuffer::loadBlockAndGetBufferPtr(&bufferPtr);
  
  if(bufferNum!=SUCCESS){
    return bufferNum;
  }


  //get the header of the block(we need number of attr and number of slots)
  HeadInfo head;
  BlockBuffer::getHeader(&head);
  

  int attrCount = head.numAttrs;
  int slotCount = head.numSlots;


  //number of slots is out of range
  if(slotNum > slotCount or slotNum < 0){
    return E_OUTOFBOUND;
  }

  //offset
  int recordSize = attrCount * ATTR_SIZE;
  unsigned char *slotPointer = bufferPtr + (32 + slotCount + (recordSize * slotNum));

  memcpy(slotPointer,rec,recordSize);

  StaticBuffer::setDirtyBit(this->blockNum);
  
  return SUCCESS;
}

