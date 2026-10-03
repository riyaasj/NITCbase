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


BlockBuffer::BlockBuffer(char blockType){

  //allocate a new block
  int blocktype;
  if(blockType == 'R'){
    blocktype = REC;
  }
  else{
    blocktype = UNUSED_BLK;
  }

  int blockNum = BlockBuffer::getFreeBlock(blockType);


  // set the blockNum field of the object to that of the allocated block number if the method returned a valid block number, otherwise set the error code returned as the block number.
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


RecBuffer::RecBuffer() : BlockBuffer('R'){}
// call parent non-default constructor with 'R' denoting record block.



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
  int bufferNum = BlockBuffer::loadBlockAndGetBufferPtr(&bufferPtr);
  
  if(bufferNum != SUCCESS){
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



int BlockBuffer::setHeader(struct HeadInfo *head){

  // get the starting address of the buffer containing the block
  unsigned char *bufferPtr;
  int blockNum = BlockBuffer::loadBlockAndGetBufferPtr(&bufferPtr);
  

  if (blockNum != SUCCESS) return blockNum;

  // cast bufferPtr to type HeadInfo*
  struct HeadInfo *bufferHeader = (struct HeadInfo *)bufferPtr;

  // copy the fields of the HeadInfo pointed to by head (except reserved) to the header of the block (pointed to by bufferHeader)
  bufferHeader->numSlots = head->numSlots;
  bufferHeader->lblock = head->lblock;
  bufferHeader->numEntries = head->numEntries;
  bufferHeader->pblock = head->pblock;
  bufferHeader->rblock = head->rblock;
  bufferHeader->blockType = head->blockType;
  bufferHeader->numAttrs=head->numAttrs;

  // update dirty bit
  int x = StaticBuffer::setDirtyBit(this->blockNum);

  // if setDirtyBit() failed, return the error code
  return x;
  // return SUCCESS;
}



int BlockBuffer::setBlockType(int blockType){

  // get the starting address of the buffer containing the block
  unsigned char *bufferPtr;
  int blockNum = BlockBuffer::loadBlockAndGetBufferPtr(&bufferPtr);
  

  if (blockNum != SUCCESS) return blockNum;


  // store the input block type in the first 4 bytes of the buffer.
  *((int32_t *)bufferPtr) = blockType;

  // update the StaticBuffer::blockAllocMap entry corresponding to the object's block number to `blockType`.
  StaticBuffer::blockAllocMap[this->blockNum] = blockType;
  
  // update dirty bit by calling StaticBuffer::setDirtyBit()
  // if setDirtyBit() failed
      // return the returned value from the call
  return StaticBuffer::setDirtyBit(this->blockNum);
  // return SUCCESS
}



int BlockBuffer::getFreeBlock(int blockType){

    // iterate through the StaticBuffer::blockAllocMap and find the block number of a free block in the disk.
    int freeBlock = 0;
    for(freeBlock; freeBlock < DISK_BLOCKS; freeBlock++){
      if(StaticBuffer::blockAllocMap[freeBlock] == UNUSED_BLK) break;
    }

    // if no block is free,
    if(freeBlock == DISK_BLOCKS) return E_DISKFULL;


    // set the object's blockNum to the block number of the free block.
    this->blockNum = freeBlock;

    // find a free buffer
    int bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);
    
    // initialize the header of the block passing a struct HeadInfo
    struct HeadInfo header;
    header.lblock = -1;
    header.pblock = -1;
    header.rblock = -1;
    header.numAttrs = 0;
    header.numEntries = 0;
    header.numSlots = 0;
    BlockBuffer::setHeader(&header);

    // update the block type of the block to the input block type using setBlockType().
    BlockBuffer::setBlockType(blockType);

    // return block number of the free block.
    return freeBlock;
}



int RecBuffer::setSlotMap(unsigned char *slotMap) {

  unsigned char *bufferPtr;
  int blockNum = BlockBuffer::loadBlockAndGetBufferPtr(&bufferPtr);
  

  if (blockNum != SUCCESS) return blockNum;
  

  //get header
  HeadInfo header;
  BlockBuffer::getHeader(&header);
  

  int numSlots = header.numSlots;

  memcpy(bufferPtr + HEADER_SIZE, slotMap, numSlots);

  int ret = StaticBuffer::setDirtyBit(this->blockNum);

  if (ret != SUCCESS)
      return ret;

  return SUCCESS;

}


int BlockBuffer::getBlockNum(){

  return this->blockNum;
}