#include <stdarg.h>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <errno.h>

#include "dryos_hal.h"

//-------------------------------------------------------------------------------------------------------------
// Common
//-------------------------------------------------------------------------------------------------------------

void* _alloc_dma_memory(size_t size) {
  return malloc(size);
}

void _free_dma_memory(void* ptr) {
  free(ptr);
}

int* errno_get_pointer_to() {
  return &errno;
}

int uart_printf(const char* pFormat, ...) {
  va_list args;
  va_start(args, pFormat);
  int ret = vprintf(pFormat, args);
  va_end(args);

  return ret;
}

void printError(const char* pErrorMsg) {
  uart_printf("errno: %d\n", *errno_get_pointer_to());
  perror(pErrorMsg);
}

int wifiConnect() {
  uart_printf("TODO: connect to wifi. Assume it worked for now.\n");

  return 0;
}

//-------------------------------------------------------------------------------------------------------------
// Sockets
//-------------------------------------------------------------------------------------------------------------

int socket_create(int domain, int type, int protocol) {
  int convDomain = 0;

  switch (domain) {
    case DRY_AF_INET: convDomain = AF_INET; break;
  }

  int convType = 0;

  switch (type) {
    case DRY_SOCK_STREAM: convType = SOCK_STREAM; break;
  }

  return socket(convDomain, convType, 0);
}

int socket_setsockopt(int sockfd, int level, int optname, DryOpt_t* optval, socklen_t optlen) {
  int convLevel = 0;
  int convOptname = 0;

  switch (level) {
    case DRY_SOL_SOCKET: convLevel = SOL_SOCKET; break;
  }

  switch (optname) {
    case DRY_SO_REUSEADDR: convOptname = SO_REUSEADDR; break;
    case DRY_SO_REUSEPORT: convOptname = SO_REUSEPORT; break;
  }

  int convOpt = optval->lo;

  return setsockopt(sockfd, convLevel, convOptname, &convOpt, sizeof(convOpt));
}

int socket_bind(int sockfd, DrySockaddr_in* addr, socklen_t addrlen) {
  struct sockaddr_in convAddr;
  convAddr.sin_family = AF_INET;
  convAddr.sin_addr.s_addr = INADDR_ANY;
  convAddr.sin_port = addr->sin_port;

  return bind(sockfd, (struct sockaddr *)&convAddr, sizeof(convAddr));
}

int socket_listen(int sockfd,int backlogl) {
  return listen(sockfd, backlogl);
}

int socket_accept(int sockfd, DrySockaddr_in* addr, socklen_t* addrlen) {
  struct sockaddr_in convAddr;
  convAddr.sin_family = AF_INET;
  convAddr.sin_port = addr->sin_port;
  convAddr.sin_addr.s_addr = INADDR_ANY;

  int convAddrlen = sizeof(convAddr);

  return accept(sockfd, (struct sockaddr*)&convAddr, (socklen_t*)&convAddrlen);
}

int socket_recv(int sockfd, void* buf, size_t len, int flags) {
  return read(sockfd, buf, len);
}

int socket_send(int sockfd, const void* buf, size_t len, int flags) {
  return send(sockfd, buf, len, flags);
}

int eosCreateServer() {
  return 0;
}

//-------------------------------------------------------------------------------------------------------------
// File I/O
//-------------------------------------------------------------------------------------------------------------

static const char* stripDriveLetter(const char* pFileName) {
  if (!pFileName)
    return 0;

  if ( (pFileName[0] != 'A' && pFileName[0] != 'B') || pFileName[1] != ':' || pFileName[2] != '/')
    return 0;

  return &pFileName[3];
}

FILE* _FIO_CreateFile(const char* pFileName) {
  return _FIO_OpenFile(pFileName, O_WRONLY);
}

FILE* _FIO_OpenFile(const char* pFileName, uint32_t mode) {
  const char* pMode = "rb";

  if (mode & O_WRONLY)
    pMode = "wb";

  if (!pMode) {
    uart_printf("_FIO_OpenFile: invalid mode 0x%X!\n", mode);

    return (FILE*)-1;
  }

  FILE* pFile = fopen(stripDriveLetter(pFileName), pMode);

  if (!pFile) {
    uart_printf("_FIO_OpenFile: failed to open file '%s'!\n", pFileName);
    return (FILE*)-1;
  }

  return pFile;
}

int FIO_CloseFile(FILE* pStream) {
  return fclose(pStream);
}

int _FIO_ReadFile(FILE* pFile, void* pBuffer, size_t count) {
  return fread(pBuffer, 1, count, pFile);
}

int _FIO_WriteFile(FILE* pStream, const void* ptr, size_t count) {
  return fwrite(ptr, 1, count, pStream);
}

int _FIO_RemoveFile(const char* pFileName) {
  return remove(stripDriveLetter(pFileName));
}

int _FIO_RenameFile(const char* pSrc, const char* pDst) {
  return rename(stripDriveLetter(pSrc), stripDriveLetter(pDst));
}

int _FIO_GetFileSize(const char* pFileName, size64_t* pSize) {
  if (!pSize)
    return -1;

  FILE* pFile = fopen(stripDriveLetter(pFileName), "rb");

  if (!pFile) {
    uart_printf("_FIO_GetFileSize: failed to open file");
    return -1;
  }

  fseek(pFile, 0L, SEEK_END);

  if ((pSize->lo = ftell(pFile)) < 0) {
    uart_printf("_FIO_GetFileSize: ftell failed\n");
    return -1;
  }

  fclose(pFile);

  return 0;
}

//-------------------------------------------------------------------------------------------------------------
// MD5
//-------------------------------------------------------------------------------------------------------------

void Md5_Init(Md5Ctx* pCtx) {

}

void Md5_Update(Md5Ctx* pCtx, void* pData, size_t size) {

}

// TODO: super dirty hack! Do proper MD5 calculation!

void Md5_Final(Md5Ctx* pCtx, uint8_t* pMd5HashOut) {
  FILE* pFile = popen("/usr/bin/md5sum autoexec.bin", "r");

  if (pFile == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  char pOutput[1024];
  fgets(pOutput, sizeof(pOutput), pFile);
  pclose(pFile);

  const char* p = pOutput;

  for (int i = 0; i < 16; ++i) {
    char pByte[3] = {0};
    pByte[0] = p[0];
    pByte[1] = p[1];

    int val;
    sscanf(pByte, "%02X", &val);

    pMd5HashOut[i] = (uint8_t)val;

    p += 2;
  }

  printf("\n");
}

int Md5_AllocAndInit(Md5Ctx** ppCtx) {
  if (!ppCtx)
    return -1;

  *ppCtx = (Md5Ctx*)malloc(sizeof(Md5Ctx));

  return 0;
}

void Md5_FinalAndFree(Md5Ctx* pMd5Ctx, uint8_t* pMd5HashOut) {
  if (!pMd5Ctx)
    return;

  Md5_Final(pMd5Ctx, pMd5HashOut);
  free(pMd5Ctx);
}

//-------------------------------------------------------------------------------------------------------------
// SHA-256
//-------------------------------------------------------------------------------------------------------------

int Sha256Init(void** ppSha256Ctx) {
  if (ppSha256Ctx)
    return -1;

  *ppSha256Ctx = malloc(228);

  return 0;
}

int ShaXUpdate(void* pCtx, void* pTransformFunction, uint8_t* pData, size_t size) {
  return 0; // TODO: implement
}

void Sha256_Transform(void* pData, uint32_t* pH) {

}

// TODO: super dirty hack! Do proper SHA-256 calculation!

int ShaXFinal(void* pCtx, void* pTransFormFunction, uint8_t* pFinalHash) {
  FILE* pFile = popen("/usr/bin/sha256sum autoexec.bin", "r");

  if (pFile == NULL) {
    printf("Failed to run command\n" );
    exit(1);
  }

  char pOutput[1024];
  fgets(pOutput, sizeof(pOutput), pFile);
  pclose(pFile);

  const char* p = pOutput;

  for (int i = 0; i < 32; ++i) {
    char pByte[3] = {0};
    pByte[0] = p[0];
    pByte[1] = p[1];

    int val;
    sscanf(pByte, "%02X", &val);

    pFinalHash[i] = (uint8_t)val;

    p += 2;
  }

  printf("\n");

  return 0;
}

void ShaXFree(void** ppSha256Ctx) {
  if (ppSha256Ctx) {
    if (*ppSha256Ctx)
      free(*ppSha256Ctx);
  }
}
