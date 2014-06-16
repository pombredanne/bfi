#include <inttypes.h>

#define BFI_MAGIC 0x053e // 1342
#define BFI_HEADER 12
#define BFI_RECORDS_PER_PAGE 512

#define BFI_ERR_MAGIC -201
#define BFI_ERR_VERSION -202
#define BFI_ERR_FORMAT -203

#define BFI_FORMAT_128 128
#define BFI_FORMAT_256 256

// a bunch of blooms in a file
typedef struct {
    // Header fields - 12 bytes
  uint16_t magic_number;
  uint8_t version;
  uint8_t unused1;
  uint16_t format;
  uint16_t unused2;
  uint32_t slots;
    // Runtime pointers
  int fp;
  char * map;
  int32_t current_page;
  int32_t total_pages;
  uint32_t page_size;
  char * page;
} bfi;

void    bfi_generate(char * input[], int items, char ** ptr, int size);
int     bfi_contains(char * haystack, char * needle, int len);

bfi*    bfi_open(char * filename, int format);
void    bfi_close(bfi * index);
uint32_t bfi_sync(bfi * index);

uint32_t bfi_append(bfi * index, char * input[], int items);
uint32_t bfi_write(bfi * index, uint32_t pk, char * input[], int items);
int     bfi_lookup(bfi * index, char * input[], int items, uint32_t ** ptr);
