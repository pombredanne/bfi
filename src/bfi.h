#include <inttypes.h>

#define BLOOM_SIZE 128
#define BFI_MAGIC 0x053e // 1342
#define BFI_HEADER 8
#define BFI_PAGE_SIZE 1000

// a bunch of blooms in a file
typedef struct {
    uint16_t    magic_number;
    uint8_t     version;
    uint8_t     unused;
    uint32_t    records;
    FILE        *fp;
    int32_t     current_page;
    uint32_t    pks[BFI_PAGE_SIZE];
    char        page[BLOOM_SIZE * BFI_PAGE_SIZE];
    char        page_dirty;
} bfi;

char*   bfi_generate(char *input[], int items);
int     bfi_contains(char *haystack, char *needle, int len);

bfi*    bfi_open(char *filename);
void    bfi_close(bfi *index);
void    bfi_sync(bfi *index);

int     bfi_index(bfi *index, int pk, char *input[], int items);
int     bfi_index_stdin(bfi *index, int row);
void    bf_lookup(bfi *index, char *input[], int items);
