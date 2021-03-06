#include <stdlib.h>
#include <string.h>

#include <ini.h>
#include <dryos_hal.h>

#include "protocol.h"

static int createServer(int serverFd, int port) {
  DryOpt_t opt;
  opt.lo = 1;

  uart_printf("set socket option1\n");

  if (socket_setsockopt(serverFd, DRY_SOL_SOCKET, DRY_SO_REUSEADDR, &opt, sizeof(opt)) < 0 ) {
    printError("setsockopt");
    return 1;
  }

  uart_printf("set socket option2\n");

  if (socket_setsockopt(serverFd, DRY_SOL_SOCKET, DRY_SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
    printError("setsockopt");
    return 1;
  }

  DrySockaddr_in address;
  address.sin_family = htons(DRY_AF_INET);
  address.sin_addr.s_addr = DRY_INADDR_ANY;
  address.sin_port = htons(port);

  uart_printf("bind at port: %d\n", port);

  if (socket_bind(serverFd, &address, sizeof(address)) < 0) {
    printError("bind failed");
    return 1;
  }

  uart_printf("listen\n");

  if (socket_listen(serverFd, 1) < 0) {
    printError("listen");
    return 1;
  }

  uart_printf("accept\n");
  uart_printf("now waiting for connection on port %d...\n", port);

  unsigned int addrlen = sizeof(address);
  int c = socket_accept(serverFd, &address, &addrlen);

  if (c < 0) {
    printError("accept");
    return 1;
  }

  uart_printf("client connected.\n");

  return c;
}

static int recvRequest(int clientFd, AnnounceFileReqMsg_t* pReq) {
  // TODO: check for TCP error:
  socket_recv(clientFd, pReq, sizeof(AnnounceFileReqMsg_t), 0);

  if (pReq->protocolVersion != 1)
    return ANNOUNCE_STATUS_UNSUPPORTED_VERSION;

  pReq->pFileName[sizeof(pReq->pFileName) -1] = 0;

  uart_printf("File announce message received: \n");
  uart_printf("File Name: %s\n", pReq->pFileName);
  uart_printf("File size: %lld\n", pReq->fileSize);
  uart_printf("SHA-256 Hash: ");

  for (int i = 0; i < sizeof(pReq->pSha256Hash); ++i)
    uart_printf("%02X", pReq->pSha256Hash[i]);

  uart_printf("\n");
  uart_printf("Protocol Version: %d\n", pReq->protocolVersion);

  return ANNOUNCE_STATUS_OK;
}

static int performUpdate(int clientFd) {
  AnnounceFileReqMsg_t req;
  AnnounceFileRspMsg_t rsp;
  rsp.status = recvRequest(clientFd, &req);
  int sent = socket_send(clientFd, &rsp, sizeof(rsp), 0);

  uart_printf("sent %d bytes\n", sent);

  if (rsp.status == ANNOUNCE_STATUS_UNSUPPORTED_VERSION) {
    uart_printf("Client protocol version is unsupported! Aborting.\n");
    return 1;
  }

  uart_printf("Now receiving file...\n");

  const char* pTempFile = "B:/FILE.TMP";

  _FIO_RemoveFile(pTempFile);

  FILE* pFile = _FIO_CreateFile(pTempFile);

  if (pFile == (FILE*)-1) {
    printError("Unable to create temporary file!\n");
    return 1;
  }

  const size_t recvBufferSize = 1024;
  uint8_t* pBuffer = _alloc_dma_memory(recvBufferSize);

  uint64_t bytesReceived;

  for (bytesReceived = 0; bytesReceived < req.fileSize;) {
    int chunkSize = socket_recv(clientFd, pBuffer, recvBufferSize, 0);

    uart_printf("received: %d\n", chunkSize);

    if (chunkSize <= 0) {
      printError("transmission failed");
      break;
    }

    uart_printf("pFile is: 0x%X. now writing\n", pFile);

    int bytesWritten;

    if ((bytesWritten = _FIO_WriteFile(pFile, pBuffer, chunkSize)) != chunkSize) {
      uart_printf("write to file failed. %d bytes written but expected to write %d!\n", bytesWritten, chunkSize);
      break;
    }

    uart_printf("written: %d\n", bytesWritten);

    bytesReceived += chunkSize;
  }

  _free_dma_memory(pBuffer);
  pBuffer = 0;

  FIO_CloseFile(pFile);

  uart_printf("Bytes received: %d\n", bytesReceived);

  if (bytesReceived != req.fileSize) {
     uart_printf("File transmission failed!\n");
     return 1;
  }

  uart_printf("File transmission finished!\n");

  int dummy = 42;
  socket_send(clientFd, &dummy, 1, 0);

  uart_printf("Now checking SHA-256\n");

  size64_t fileSize64 = {0};

  if (_FIO_GetFileSize(pTempFile, &fileSize64) == -1) {
    uart_printf("failed to get file size! aborting\n");
    return 1;
  }

  uint32_t fileSize = fileSize64.lo; // TODO: will break on files bigger than 4GB! Fix!

  uart_printf("file size of reopened file is: %d (lo: %d, hi: %d)\n", fileSize, fileSize64.lo, fileSize64.hi);

  pBuffer = _alloc_dma_memory(recvBufferSize);

  if (!pBuffer) {
    uart_printf("failed to create SHA-256 working buffer!\n");
    return 0;
  }

  uart_printf("Start SHA-256 calc\n");

  pFile = _FIO_OpenFile(pTempFile, O_RDONLY);

  if (pFile == (FILE*)-1) {
    printError("Unable to reopen temporary file!\n");
    return 1;
  }

  uart_printf("reopened pFile=%X\n", pFile);

  int error;

  void* pSha256Ctx = 0;
  if ((error = Sha256Init(&pSha256Ctx))) {
    uart_printf("Error on Sha256Init: %d\n", error);
    return 1;
  }

  int bytesRead;
  for (bytesRead = 0; bytesRead < fileSize;) {
    int chunkSize = _FIO_ReadFile(pFile, pBuffer, recvBufferSize);

    if (chunkSize < 0) {
      uart_printf("error on reading file during SHA-256 check!\n");
      return 1;
    }

    uart_printf("read: %d\n", chunkSize);
    ShaXUpdate(pSha256Ctx, Sha256_Transform, pBuffer, chunkSize);

    bytesRead += chunkSize;
  }

  _free_dma_memory(pBuffer);
  FIO_CloseFile(pFile);

  uint8_t pSha256Hash[32];
  ShaXFinal(pSha256Ctx, Sha256_Transform, pSha256Hash);
  ShaXFree(&pSha256Ctx);

  uart_printf("SHA-256 calc finish\n");
  uart_printf("calculated: ");

  for (int i = 0; i < sizeof(pSha256Hash); ++i)
    uart_printf("%02X", pSha256Hash[i]);

  uart_printf("\n");

  uart_printf("expected: ");

  for (int i = 0; i < sizeof(pSha256Hash); ++i)
    uart_printf("%02X", req.pSha256Hash[i]);

  uart_printf("\n");

  for (int i = 0; i < sizeof(pSha256Hash); ++i) {
    if (pSha256Hash[i] != req.pSha256Hash[i]) {
      uart_printf("Error: SHA-256 checksum mismatch!\n");
      return -1;
    }
  }

  uart_printf("checksum ok\n");

  char pTargetFileName[128];
  snprintf(pTargetFileName, sizeof(pTargetFileName), "B:/%s", req.pFileName);

  _FIO_RemoveFile(pTargetFileName);

  if ((error = _FIO_RenameFile(pTempFile, pTargetFileName)) != 0) {
    uart_printf("[%d] Error on rename '%s' -> '%s'!\n", error, pTempFile, pTargetFileName);
    return 1;
  }

  return 0;
}

typedef struct {
  char pIp[16];
  WlanSettings_t settings;
} UpdaterConfig_t;

static int handler(void* pUser, const char* pSection, const char* pName, const char* pValue) {
  UpdaterConfig_t* pConfig = (UpdaterConfig_t*)pUser;

  #define MATCH(s, n) strcmp(pSection, s) == 0 && strcmp(pName, n) == 0
  if (MATCH("wifi", "ip"))
    strncpy(pConfig->pIp, pValue, sizeof(pConfig->pIp));
  else if (MATCH("wifi", "ssid"))
    strncpy(pConfig->settings.pSSID, pValue, sizeof(pConfig->settings.pSSID));
  else if (MATCH("wifi", "password"))
    strncpy(pConfig->settings.pKey, pValue, sizeof(pConfig->settings.pKey));
  else if (MATCH("wifi", "channel"))
    pConfig->settings.channel = atoi(pValue);
  else if (MATCH("wifi", "authmode")) {
    if (strncmp(pValue, "open", 16) == 0)
      pConfig->settings.authMode = OPEN;
    else if (strncmp(pValue, "shared", 16) == 0)
      pConfig->settings.authMode = SHARED;
    else if (strncmp(pValue, "wpa2psk", 16) == 0)
      pConfig->settings.authMode = WPA2PSK;
    else if (strncmp(pValue, "both", 16) == 0)
      pConfig->settings.authMode = BOTH;
  }
  else if (MATCH("wifi", "ciphermode")) {
    if (strncmp(pValue, "none", 16) == 0)
      pConfig->settings.cipherMode = NONE;
    else if (strncmp(pValue, "wep", 16) == 0)
      pConfig->settings.cipherMode = WEP;
    else if (strncmp(pValue, "aes", 16) == 0)
      pConfig->settings.cipherMode = AES;
  }
  else
    return 0; // unknown section/name, error

  return 1;
}

static int wifiConnect() {
  UpdaterConfig_t config;
  memset(&config, 0, sizeof(UpdaterConfig_t));

  const char* pConfigFileName = "wificonfig.ini";

  if (ini_parse(pConfigFileName, handler, &config) < 0) {
    uart_printf("Can't load '%s'\n", pConfigFileName);
    return 1;
  }

  config.settings.mode = INFRA;
  config.settings.g = 6; // set to 6 when using INFRA or BOTH

  uart_printf("Config loaded from 'wificonfig.ini': \n"
    "ssid=%s\n"
    "password=%s\n"
    "channel=%d\n"
    "authMode=%d\n"
    "cipherMode=%d\n",
    config.settings.pSSID, config.settings.pKey, config.settings.channel,
    config.settings.authMode, config.settings.cipherMode);

  uart_printf("exec NwLimeOn\n");
  call("NwLimeOn");
  uart_printf("exec wlanchk\n");
  call("wlanchk");

  uart_printf("now connecting to '%s' AP...\n", config.settings.pSSID);
  int wlanResult = wlanconnect(&config.settings);
  uart_printf("wlan connect result: %d\n", wlanResult);

  if (wlanResult != 0)
    return wlanResult;

  uart_printf("set ip to: %s\n", config.pIp);
  call("wlanipset", config.pIp);

  return 0;
}

int drysh_ml_update(int argc, char const *argv[]) {
  int error;

  error = wifiConnect();

  if (error) {
    printError("connection to wifi failed!\n");
    return 1;
  }

  uart_printf("creating socket\n");

  int serverFd;

  if ((serverFd = socket_create(DRY_AF_INET, DRY_SOCK_STREAM, 0)) < 0) {
    printError("socket failed");
    return 1;
  }

  int clientFd = createServer(serverFd, 2342);

  if (clientFd < 0) {
    printError("Failed to create server");
    return 1;
  }

  if ((error = performUpdate(clientFd)) != 0) {
    printError("Update failed!");
    return 1;
  }

  uart_printf("Update successful!\n");
  uart_printf("Rebooting...\n");

  reboot();

  return 0;
}

int main(int argc, char const* pArgv[]) {
  return drysh_ml_update(argc, pArgv);
}

