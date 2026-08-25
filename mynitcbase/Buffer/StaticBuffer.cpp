#include "StaticBuffer.h"

unsigned char StaticBuffer::blocks[BUFFER_CAPACITY][BLOCK_SIZE];
struct BufferMetaInfo StaticBuffer::metainfo[BUFFER_CAPACITY];

/*
  methods implemented:
    StaticBuffer()
    ~StaticBuffer()
    getFreeBuffer(int blockNum)
    getBufferNum(int blockNum)
*/


StaticBuffer::StaticBuffer() {

  // initialise all blocks as free
  for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
    metainfo[bufferIndex].free = true;
  }
}


//empty destructor since we are not writing back from the buffer to the disk
StaticBuffer::~StaticBuffer() {}



//get free buffer block
int StaticBuffer::getFreeBuffer(int blockNum) {
  if (blockNum < 0 || blockNum > DISK_BLOCKS) {
    return E_OUTOFBOUND;
  }

  int allocatedBuffer;

  // iterate through all the blocks and find the first free block in the buffer (check metainfo)
  for (allocatedBuffer = 0; allocatedBuffer < BUFFER_CAPACITY; allocatedBuffer++) {
    if(metainfo[allocatedBuffer].free == true) break;
  }

  metainfo[allocatedBuffer].free = false;
  metainfo[allocatedBuffer].blockNum = blockNum;

  return allocatedBuffer;
}



//Get the buffer index where a particular block is stored
int StaticBuffer::getBufferNum(int blockNum) {

  //Check if blockNum is valid
  if (blockNum < 0 || blockNum > DISK_BLOCKS) {
    return E_OUTOFBOUND;
  }

  //find and return the bufferIndex which corresponds to blockNum
  for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
    if(metainfo[bufferIndex].blockNum == blockNum) return bufferIndex;
  }

  //if block is not in the buffer
  return E_BLOCKNOTINBUFFER;
}
