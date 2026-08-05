#include <iostream>
#include <cstring>
#include "Buffer/StaticBuffer.h"
#include "Cache/OpenRelTable.h"
#include "Disk_Class/Disk.h"
#include "FrontendInterface/FrontendInterface.h"

int main(int argc, char *argv[]) {

  Disk disk_run;

  //read from block 7000
  unsigned char buffer[BLOCK_SIZE];
  Disk::readBlock(buffer, 7000);

  //write into block 7000
  char message[] = "hello";
  memcpy(buffer + 20, message, 6);
  Disk::writeBlock(buffer, 7000);

  
  unsigned char buffer2[BLOCK_SIZE];
  char message2[6];
  Disk::readBlock(buffer2, 7000);
  memcpy(message2, buffer2 + 20, 6);
  std::cout << message2;
  
/*
  unsigned char buffer2[BLOCK_SIZE];
  Disk::readBlock(buffer2, 0);

  for (int i = 0; i < 20; i++) {
      std::cout << (int)buffer2[i] << " ";
  }
  std::cout << std::endl;
*/

  return 0;
}