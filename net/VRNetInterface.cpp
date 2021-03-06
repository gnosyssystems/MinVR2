#include "VRNetInterface.h"

#include "ByteStream.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

// unique identifiers for different network messages
const unsigned char VRNetInterface::INPUT_EVENTS_MSG = 1;
const unsigned char VRNetInterface::SWAP_BUFFERS_REQUEST_MSG = 2;
const unsigned char VRNetInterface::SWAP_BUFFERS_NOW_MSG = 3;

void 
VRNetInterface::sendSwapBuffersRequest(int socketID) {
  // this message consists only of a 1-byte header
  sendall(socketID, &SWAP_BUFFERS_REQUEST_MSG, 1);
}

void 
VRNetInterface::sendSwapBuffersNow(int socketID) {
  // this message consists only of a 1-byte header
  sendall(socketID, &SWAP_BUFFERS_NOW_MSG, 1);
}

void 
VRNetInterface::sendInputEvents(int socketID, std::vector<VREvent> &inputEvents) {    
  // 1. send 1-byte message header
  sendall(socketID, &INPUT_EVENTS_MSG, 1);
  
  // determine size of message data in bytes = size of all input events + one int to store the number of input events
  int dataSize = 0;
  for (std::vector<VREvent>::iterator itr=inputEvents.begin(); itr<inputEvents.end(); itr++) {
    dataSize += itr->getSizeInBytes();
  }
  dataSize += ByteData::BYTEDATA_SIZEOFINT;
  
  // 2. send the size of the message data so receive will know how many bytes to expect
  unsigned char buf[ByteData::BYTEDATA_SIZEOFINT];
  ByteData::packInt(buf, dataSize);
  sendall(socketID, buf, ByteData::BYTEDATA_SIZEOFINT);
  
  // 3. pack all the input events data and send these bytes
  ByteStream bs(dataSize);
  bs.writeInt(inputEvents.size());
  for (std::vector<VREvent>::iterator itr=inputEvents.begin(); itr<inputEvents.end(); itr++) {
    itr->serialize(bs);
  }
  sendall(socketID, bs.getByteArray(), bs.getSize());
  
  if (dataSize != bs.getSize()) {
    std::cerr << "problem: datasize calculated incorrectly:" << dataSize << " " << bs.getSize() << std::endl;
  }
}


int 
VRNetInterface::sendall(int s, const unsigned char *buf, int len) {
  int total = 0;        // how many bytes we've sent
  int bytesleft = len; // how many we have left to send
  int n;    
  while (total < len) {
    n = send(s, buf+total, bytesleft, 0);
    if (n == -1) { break; }
    total += n;
    bytesleft -= n;
  }
  return n==-1?-1:total; // return -1 on failure, total on success
} 




void 
VRNetInterface::waitForAndReceiveMessageHeader(int socketID, unsigned char messageID) {
  unsigned char receivedID = 0x0;
  while (receivedID != messageID) {
    int status = receiveall(socketID, &receivedID, 1);
    if (status == -1) {
      std::cerr << "NetInterface error: receiveall failed." << std::endl;
      exit(1);
    }
    else if ((status == 1) && (receivedID != messageID)) {
      std::cerr << "NetInterface error, unexpected message.  Expected: " << (int)messageID << " Received: " << (int)receivedID << std::endl;
    }
  }
}

void 
VRNetInterface::waitForAndReceiveSwapBuffersRequest(int socketID) {
  // this message consists only of a 1-byte header
  waitForAndReceiveMessageHeader(socketID, SWAP_BUFFERS_REQUEST_MSG);
}

void 
VRNetInterface::waitForAndReceiveSwapBuffersNow(int socketID) {
  // this message consists only of a 1-byte header
  waitForAndReceiveMessageHeader(socketID, SWAP_BUFFERS_NOW_MSG);
}


void 
VRNetInterface::waitForAndReceiveInputEvents(int socketID, std::vector<VREvent> &inputEvents) {
  // 1. receive 1-byte message header
  waitForAndReceiveMessageHeader(socketID, INPUT_EVENTS_MSG);
  
  // 2. receive int that tells us the size of the data portion of the message in bytes
  unsigned char buf1[ByteData::BYTEDATA_SIZEOFINT];
  int status = receiveall(socketID, buf1, ByteData::BYTEDATA_SIZEOFINT);
  if (status == -1) {
    std::cerr << "NetInterface error: receiveall failed." << std::endl;
    exit(1);
  }
  int dataSize = ByteData::unpackInt(buf1);
  
  // 3. receive dataSize bytes, then decode these as InputEvents
  unsigned char *buf2 = new unsigned char[dataSize];
  status = receiveall(socketID, buf2, dataSize);
  if (status == -1) {
    std::cerr << "NetInterface error: receiveall failed." << std::endl;
    exit(1);
  }
  // the first few bytes are an int telling us how many events to expect
  int numEvents = ByteData::unpackInt(buf2);
  // create a bytestream out of the remaining bytes
  ByteStream bs(buf2 + ByteData::BYTEDATA_SIZEOFINT, dataSize - ByteData::BYTEDATA_SIZEOFINT);
  delete [] buf2;
  for (int i=0; i<numEvents; i++) {
    std::cout << i << std::endl;
    VREvent e;
    e.deserialize(bs);
    inputEvents.push_back(e);
  }
}

int 
VRNetInterface::receiveall(int s, unsigned char *buf, int len) {
  int total = 0;        // how many bytes we've received
  int bytesleft = len; // how many we have left to receive
  int n;    
  while (total < len) {
    n = recv(s, buf+total, bytesleft, 0);
    if (n == -1) { break; }
    total += n;
    bytesleft -= n;
  }
  return n==-1?-1:total; // return -1 on failure, total on success
}
