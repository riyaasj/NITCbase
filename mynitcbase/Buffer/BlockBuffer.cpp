#include "BlockBuffer.h"
#include<stdio.h>
#include <cstdlib>
#include <cstring>

BlockBuffer::BlockBuffer(int blockNum) {
    this->blockNum = blockNum;
}

BlockBuffer::BlockBuffer(char blockType) {
    int block_type = blockType == 'R'? REC: UNUSED_BLK;
    int blockNum = getFreeBlock(block_type);
    if(blockNum < 0|| blockNum >= DISK_BLOCKS){
        printf("Block not available.\n");
        this->blockNum = blockNum;
        return;
    } 
    this->blockNum = blockNum;
}

RecBuffer::RecBuffer(int blockNum) : BlockBuffer::BlockBuffer(blockNum) {} /* Constructor Initializer*/
RecBuffer::RecBuffer() : BlockBuffer::BlockBuffer('R') {}

int BlockBuffer::getHeader(struct HeadInfo* head) {
    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS) {
    return ret;   // return any errors that might have occured in the process
    }

    memcpy(&head->numSlots, bufferPtr+24, 4);
    memcpy(&head->numAttrs, bufferPtr+20, 4);
    memcpy(&head->numEntries, bufferPtr+16, 4);
    memcpy(&head->rblock, bufferPtr+12, 4);
    memcpy(&head->lblock, bufferPtr+8, 4);
    memcpy(&head->pblock, bufferPtr+4, 4);

    return SUCCESS;
}

int BlockBuffer::setHeader(struct HeadInfo* head) {
    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS) {
    return ret;   // return any errors that might have occured in the process
    }

    struct HeadInfo *bufferHeader= (struct HeadInfo *)bufferPtr;
    bufferHeader->blockType = head->blockType;
    bufferHeader->pblock = head->pblock;
    bufferHeader->rblock = head->rblock;
    bufferHeader->lblock = head->lblock;
    bufferHeader->numEntries = head->numEntries;
    bufferHeader->numAttrs = head->numAttrs;
    bufferHeader->numSlots = head->numSlots;

    int res = StaticBuffer::setDirtyBit(this->blockNum);
    if(res == E_BLOCKNOTINBUFFER) {
        return E_BLOCKNOTINBUFFER;
    }
    if(res == E_OUTOFBOUND) {
        return E_OUTOFBOUND;
    }
    return SUCCESS;

}

int BlockBuffer::getFreeBlock(int blockType) {

    //finds free block 
    int freeBlockNum = -1;
    for(int i = 0; i < DISK_BLOCKS; i++) {
        if(StaticBuffer::blockAllocMap[i] == UNUSED_BLK) {
            freeBlockNum = i;
            break;
        }
    }
    if(freeBlockNum == -1) {
        return E_DISKFULL;
    }
    this->blockNum = freeBlockNum;
    

    //puts into buffer
    int bufferNum = StaticBuffer::getFreeBuffer(blockNum);
     if(bufferNum < 0 || bufferNum >= BUFFER_CAPACITY) {
        return bufferNum;
    }

    struct HeadInfo header;
    header.lblock = -1;
    header.rblock = -1;
    header.pblock = -1;
    header.numAttrs = 0;
    header.numEntries = 0;
    header.numSlots = 0;
    setHeader(&header);

    setBlockType(blockType);
    return freeBlockNum;
}



int RecBuffer::getRecord(union Attribute *rec, int slotNum) {
    HeadInfo head;

    BlockBuffer::getHeader(&head);

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

     unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS) {
    return ret;
    }

    int recordStart = HEADER_SIZE + slotCount + slotNum * (ATTR_SIZE * attrCount);
    int recordSize = ATTR_SIZE * attrCount;

    unsigned char *start = bufferPtr + recordStart;

    memcpy(rec, start, recordSize);
    return SUCCESS;
}

int RecBuffer::setRecord(union Attribute *rec, int slotNum) {
     unsigned char *bufferPtr;

    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS)
    {
        return ret;
    }
    HeadInfo head;

    BlockBuffer::getHeader(&head);

    int attrCount = head.numAttrs;
    int slotCount = head.numSlots;

    int recordSize = ATTR_SIZE * attrCount;
    int recordStart = HEADER_SIZE + slotCount + slotNum * recordSize;
    unsigned char *start = bufferPtr + recordStart;

    memcpy(start, rec, recordSize);

    if(StaticBuffer::setDirtyBit(this->blockNum) != SUCCESS) {
        printf("Setting Dirty Failed.\n");
    }

    return SUCCESS;
}

int BlockBuffer::loadBlockAndGetBufferPtr(unsigned char **buffPtr){

  // check whether the block is already present in the buffer using StaticBuffer.getBufferNum()
  int bufferNum = StaticBuffer::getBufferNum(this->blockNum);
  if(bufferNum != E_BLOCKNOTINBUFFER) {
        for(int i = 0; i < BUFFER_CAPACITY; i++) {
            StaticBuffer::metainfo[i].timeStamp++;
        }
        StaticBuffer::metainfo[bufferNum].timeStamp = 0;
    }
  else {
    bufferNum = StaticBuffer::getFreeBuffer(this->blockNum);

    if (bufferNum == E_OUTOFBOUND) {
      return E_OUTOFBOUND;
    }

    Disk::readBlock(StaticBuffer::blocks[bufferNum], this->blockNum);
  }
   // store the pointer to this buffer (blocks[bufferNum]) in *buffPtr
  *buffPtr = StaticBuffer::blocks[bufferNum];

  return SUCCESS;
}

/* used to get the slotmap from a record block
NOTE: this function expects the caller to allocate memory for `*slotMap`
*/
int RecBuffer::getSlotMap(unsigned char *slotMap) {
  unsigned char *bufferPtr;

  // get the starting address of the buffer containing the block using loadBlockAndGetBufferPtr().
  int ret = loadBlockAndGetBufferPtr(&bufferPtr);
  if (ret != SUCCESS) {
    return ret;
  }

  //Read the header of the block using BlockBuffer::getHeader() and store it in a HeadInfo struct
  struct HeadInfo head;
  BlockBuffer::getHeader(&head);

  //Count the number of slots in the block using head.numSlots and copy the slotmap from the buffer to the slotMap argument
  int slotCount = head.numSlots;
  unsigned char *slotMapInBuffer = bufferPtr + HEADER_SIZE;

  memcpy(slotMap, slotMapInBuffer, slotCount);
    return SUCCESS;
}

int RecBuffer::setSlotMap(unsigned char* slotMap) {
    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if(ret != SUCCESS) {
        return ret;
    }
    HeadInfo header;
    getHeader(&header);
    int numSlots = header.numSlots;
    memcpy(bufferPtr + HEADER_SIZE, slotMap, numSlots);
    ret = StaticBuffer::setDirtyBit(this->blockNum);

    return SUCCESS;
}



int BlockBuffer::setBlockType(int blockType){
    unsigned char *bufferPtr;
    int ret = loadBlockAndGetBufferPtr(&bufferPtr);
    if (ret != SUCCESS) {
        return ret;   // return any errors that might have occured in the process
    }
    *((int32_t*)bufferPtr) = blockType;
    StaticBuffer::blockAllocMap[this->blockNum] = blockType;
    int res = StaticBuffer::setDirtyBit(this->blockNum);
    if(res == E_BLOCKNOTINBUFFER){
        return E_BLOCKNOTINBUFFER;
    }
    if(res == E_OUTOFBOUND){
        return E_OUTOFBOUND;
    }
    return SUCCESS;
}

int BlockBuffer::getBlockNum() {
    return this->blockNum;
}

void BlockBuffer::releaseBlock(){
    if(blockNum==-1||StaticBuffer::blockAllocMap[blockNum]==UNUSED_BLK){
        return;
    }
    int bufferNum = StaticBuffer::getBufferNum(this->blockNum);

    if(bufferNum>=0 && bufferNum<BUFFER_CAPACITY) {
        StaticBuffer::metainfo[bufferNum].free = true;
    }

    StaticBuffer::blockAllocMap[blockNum] = UNUSED_BLK;
    this->blockNum = -1;
}

int compareAttrs(union Attribute attr1, union Attribute attr2, int attrType) {

    double diff;
    
    if(attrType == STRING){
        diff = strcmp(attr1.sVal, attr2.sVal);
    }
    else if(attrType == NUMBER){
        diff = attr1.nVal - attr2.nVal;
    }
    else{
        return E_INVALID;
    }
    /*
    if diff > 0 then return 1
    if diff < 0 then return -1
    if diff = 0 then return 0
    */
   if(diff > 0){
        return 1;
    }
    else if(diff < 0){
        return -1;
    }
    else{
        return 0;
    }
}
