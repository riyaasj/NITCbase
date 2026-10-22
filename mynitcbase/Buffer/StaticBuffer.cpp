#include "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];
unsigned char StaticBuffer::blockAllocMap[DISK_BLOCKS];

/*
  methods implemented:
    StaticBuffer()
    ~StaticBuffer()
    getFreeBuffer(int blockNum)
    getBufferNum(int blockNum)
*/


StaticBuffer::StaticBuffer() {

  // copy blockAllocMap blocks from disk to buffer
  int bmapSlot = 0;
  for(int i = 0; i <= 3; i++){
    unsigned char buffer[BLOCK_SIZE];
    Disk::readBlock(buffer, i);
    for(int x = 0; x < BLOCK_SIZE; x++){
      StaticBuffer::blockAllocMap[bmapSlot++] = buffer[x];
    }
  }

  // initialise all blocks
  for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
    metainfo[bufferIndex].free = true;
    metainfo[bufferIndex].dirty = false;
    metainfo[bufferIndex].timeStamp = -1;
    metainfo[bufferIndex].blockNum = -1;
  }

}



StaticBuffer::~StaticBuffer() {

  int bmapSlot = 0;
  for(int i = 0; i <= 3; i++){
    unsigned char buffer[BLOCK_SIZE];
    for(int x = 0; x < BLOCK_SIZE; x++){
      buffer[x] = StaticBuffer::blockAllocMap[bmapSlot++];
    }
    Disk::writeBlock(buffer, i);
  }

  for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
    if(metainfo[bufferIndex].free == false and metainfo[bufferIndex].dirty == true){
      Disk::writeBlock(blocks[bufferIndex], metainfo[bufferIndex].blockNum);
    }
  }
}



//get free buffer block
int StaticBuffer::getFreeBuffer(int blockNum) {
  if (blockNum < 0 || blockNum >= DISK_BLOCKS) {
      return E_OUTOFBOUND;
  }

  int allocatedBuffer = -1;
  int oldestBuffer = -1;
  int maxTimeStamp = -1;

  for (int i = 0; i < BUFFER_CAPACITY; i++) {
      if (metainfo[i].free) {
          allocatedBuffer = i;
          break;
      }

      if (metainfo[i].timeStamp > maxTimeStamp) {
          maxTimeStamp = metainfo[i].timeStamp;
          oldestBuffer = i;
      }
  }

  // If no free buffer exists, evict the oldest one.
  if (allocatedBuffer == -1) {
      if (oldestBuffer == -1) {
          return E_BLOCKNOTINBUFFER;
      }

      if (metainfo[oldestBuffer].dirty) {
          Disk::writeBlock(
              blocks[oldestBuffer],
              metainfo[oldestBuffer].blockNum
          );
      }

      allocatedBuffer = oldestBuffer;
  }

  metainfo[allocatedBuffer].free = false;
  metainfo[allocatedBuffer].dirty = false;
  metainfo[allocatedBuffer].blockNum = blockNum;
  metainfo[allocatedBuffer].timeStamp = 0;

  return allocatedBuffer;
}



//Get the buffer index where a particular block is stored
int StaticBuffer::getBufferNum(int blockNum) {

  //Check if blockNum is valid
  if (blockNum < 0 || blockNum >= DISK_BLOCKS) {
    return E_OUTOFBOUND;
  }

  //find and return the bufferIndex which corresponds to blockNum
  for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
    if(metainfo[bufferIndex].blockNum == blockNum) return bufferIndex;
  }

  //if block is not in the buffer
  return E_BLOCKNOTINBUFFER;
}


int StaticBuffer::setDirtyBit(int blockNum){
    // find the buffer index corresponding to the block 
    int bufferIndex = StaticBuffer::getBufferNum(blockNum);

    //if block not in buffer
    if(bufferIndex == E_BLOCKNOTINBUFFER) return E_BLOCKNOTINBUFFER;

    //blockNum is out of bound
    if(bufferIndex == E_OUTOFBOUND) return E_OUTOFBOUND;

    metainfo[bufferIndex].dirty = true;
    
    return SUCCESS;
}


int StaticBuffer::getStaticBlockType(int blockNum){
    if(blockNum < 0 || blockNum >= DISK_BLOCKS){
        return E_OUTOFBOUND;
    }

    /*
        Access the entry in block allocation map corresponding to the blockNum argument
        and return the block type after type casting to integer
    */
    return (int)blockAllocMap[blockNum];
}