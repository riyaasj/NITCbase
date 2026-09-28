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

  // initialise all blocks
  for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
    metainfo[bufferIndex].free = true;
    metainfo[bufferIndex].dirty = false;
    metainfo[bufferIndex].timeStamp = -1;
    metainfo[bufferIndex].blockNum = -1;
  }

}



StaticBuffer::~StaticBuffer() {
  for (int bufferIndex = 0; bufferIndex < BUFFER_CAPACITY; bufferIndex++) {
    if(metainfo[bufferIndex].free == false and metainfo[bufferIndex].dirty == true){
      Disk::writeBlock(blocks[bufferIndex], metainfo[bufferIndex].blockNum);
    }
  }
}



//get free buffer block
int StaticBuffer::getFreeBuffer(int blockNum) {
  if (blockNum < 0 || blockNum > DISK_BLOCKS) {
    return E_OUTOFBOUND;
  }


  int allocatedBuffer = -1, maxTimeStamp = -1, b = -1;

  for(int i = 0; i < BUFFER_CAPACITY; i++){

    if(metainfo[i].free == false && metainfo[i].timeStamp > maxTimeStamp){
      b = i;
      maxTimeStamp = metainfo[i].timeStamp;
    }
    
    if(metainfo[i].free == true){
      allocatedBuffer = i;
      break;
    }
     
  }

  if(allocatedBuffer == -1){
    if(metainfo[b].dirty == true){
      Disk::writeBlock(blocks[b], metainfo[b].blockNum);
      allocatedBuffer = b;
    }
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