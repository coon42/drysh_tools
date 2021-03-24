#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

#include <dryos_hal.h>
#include "protocol.h"

int main(int argc, char const* pArgv[]) {
  const char *pProgramName = "client";

  if(argc < 2) {
    printf("Usage: %s <host name or IP>\n", pProgramName);

    return 1;
  }

  const char* pHostName = pArgv[1];

  int clientFd;

  if ((clientFd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    printf("\n Socket creation error \n");
    return -1;
  }

  // Resolve DNS name:
  struct hostent* pHostend;

  if ((pHostend = gethostbyname(pHostName)) == NULL) {
    herror("gethostbyname");
    return 1;
  }

  struct in_addr** ppAddrList = (struct in_addr**)pHostend->h_addr_list;

  char pServerIp[64];

  for(int i = 0; ppAddrList[i] != NULL; i++)
    strcpy(pServerIp , inet_ntoa(*ppAddrList[i]));

  printf("'%s' resolved to: %s\n", pHostName, pServerIp);
  // -- end of resolve

  struct sockaddr_in serv_addr;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(2342);

  if(inet_pton(AF_INET, pServerIp, &serv_addr.sin_addr)<=0) {
    printf("\nInvalid address/ Address not supported \n");
    return -1;
  }

  if (connect(clientFd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    printf("\nConnection Failed \n");
    return -1;
  }

  const char* pFileName = "AUTOEXEC.BIN";

  FILE* pFile = fopen(pFileName, "rb");

  if (!pFile) {
    printf("Error: file '%s' not found!\n", pFileName);
    return -1;
  }

  fseek(pFile, 0L, SEEK_END);
  int fileSize = ftell(pFile);
  rewind(pFile);

  AnnounceFileReqMsg_t msg;
  strncpy(msg.pFileName, pFileName, sizeof(msg.pFileName));
  msg.fileSize = fileSize;

  ShaXFinal(0, 0, msg.pSha256Hash); // TODO: dirty hack! calculate SHA-256 hash properly!

  msg.protocolVersion = 1;

  send(clientFd, &msg, sizeof(msg), 0);
  printf("Announce File Msg sent\n");

  AnnounceFileRspMsg_t rsp;
  int valread = read(clientFd, &rsp, sizeof(AnnounceFileRspMsg_t));

  printf("[Bytes recevied: %d]: Status: %d\n", valread, rsp.status);

  if (rsp.status != ANNOUNCE_STATUS_OK) {
    fclose(pFile);
    printf("Update failed! Error from server: %d\n", rsp.status);
    return 1;
  }

  printf("Now transmitting...\n");

  char pBuffer[1024] = {0};

  int bytesSent;

  for (bytesSent = 0; bytesSent < fileSize;) {
    int chunkSize = fread(pBuffer, 1, sizeof(pBuffer), pFile);

    int sent = send(clientFd, pBuffer, chunkSize, 0);

    printf("sent: %d\n", sent);

    if (sent < 0) {
      printf("Error while sending!\n");
      return 1;
    }

    bytesSent += sent;
  }

  printf("Sent %d bytes.\n", bytesSent);

  printf("waiting for sync with server\n");

  int r = read(clientFd, pBuffer, 100);

  if (r == 0 || pBuffer[0] != 42) {
    printf("Sync error!\n");
    return 1;
  }

  printf("Transmission complete!\n");

  fclose(pFile);

  return 0;
}

