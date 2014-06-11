#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include "murmur.h"
#include "bfi.h"

#define BFI_VERSION 0x03

void    bfi_release_map(bfi * index);
void    bfi_load_mapped_page(bfi * index, int page);
int     bfi_write_index(bfi * index, int offset, int pk, char * data);

/**
 * A super-simple bloom filter implementation optomised for alignment and efficiency
 *
 * Uses a fixed length of 1024 bits with 4 sectors which according to my possibly
 * dodgy maths leads to the following probabilities of error.
 * 10 fields: 2.15e-6
 * 20 fields: 3.19e-5
 * 30 fields: 1.50e-4
 *
 * The approximate target was 0.0001 so this holds till over 30 fields are indexed.
 */
void bfi_generate(char * input[], int items, char ** ptr) {
    // use 128 bytes and 12 sectors
    uint32_t hash;
    uint32_t i, offset, pos;
    char * bloom;

    //for(i=0; i<items; i++) printf("GENERATE: %s\n", input[i]);

    bloom = malloc(BLOOM_SIZE);
    if(bloom == NULL) {
        perror("Failed to allocate memory for bloom filter");
        exit(EXIT_FAILURE);
    }
    memset(bloom, 0, BLOOM_SIZE);

    for(i=0; i<items; i++) {
        //printf("VALUE: %s\n", input[i]);
        hash = murmur3_32(input[i], strlen(input[i]), 362582);
        for(offset=0; offset<4; offset++) {
            pos = 0xFF & hash;
            //printf("POS: %d\n", pos);
            hash >>= 8;
            bloom[(offset * 32) + (pos / 8)] |= (1 << (pos % 8));
        }
    }

    // for(i=0; i<128; i++) printf("%02x ", bloom[i]);
    //printf("\n");
    *ptr = bloom;
}

/**
 * Checks whether one bloom filter is within another
 */
int bfi_contains(char * haystack, char * needle, int len) {
    int i;

    for(i=0; i<len; i++) {
        //printf("%02x=%02x ", haystack[i] & needle[i], needle[i]);
        if((haystack[i] & needle[i]) != needle[i]) return 0;
    }
    return 1;
}

/**
 * Opens a bloom index and returns a resource pointer.
 * Creates the file if not already present.
 */
bfi * bfi_open(char * filename, int format) {
    bfi * result;
    int i;

    errno = 0;

    //fprintf(stderr, "OPEN %s\n", filename);

    result = malloc(sizeof(bfi));
    if(result == NULL) {
        return NULL;
    }

    result->fp = open(filename, O_RDWR | O_CREAT, (mode_t)0600);
    if(result->fp == -1) {
        return NULL;
    }

    lseek(result->fp, 0, 0);
    i = read(result->fp, result, BFI_HEADER);
    if(i == 0) {
        //fprintf(stderr, "Creating new file\n");
        result->magic_number = BFI_MAGIC;
        result->version = BFI_VERSION;
        result->format = format;
        result->records = 0;
        result->deleted = 0;
        write(result->fp, result, BFI_HEADER);
    }

    if(result->magic_number != BFI_MAGIC) {
        errno = BFI_ERR_MAGIC;
        return NULL;
    }

    if(result->version != BFI_VERSION) {
        errno = BFI_ERR_VERSION;
        return NULL;
    }

    if(result->format != format) {
      errno = BFI_ERR_FORMAT;
      return NULL;
    }

    result->map = NULL;
    result->current_page = -1;
    result->total_pages = result->records ? (result->records / BFI_RECORDS_PER_PAGE) + 1 : 0;

    return result;
}

/**
 * Flush in memory changes to disk
 */
int bfi_sync(bfi * index) {

    if(index->map == NULL) return index->records;

    // write the current page to disk
    msync(&index->map[BFI_HEADER + (BFI_PAGE_SIZE * index->current_page)], BFI_PAGE_SIZE, MS_SYNC);

    memcpy(index->map, index, BFI_HEADER);
    msync(index->map, BFI_HEADER, MS_SYNC);

    return index->records;
}

/**
 * Close resource and free memory
 */
void bfi_close(bfi * index) {
    bfi_sync(index);
    bfi_release_map(index);

    close(index->fp);

    free(index);
}

/**
 * Un map the file
 */
void bfi_release_map(bfi * index) {
    if(index->map != NULL) {
        int size = BFI_HEADER + (index->total_pages * BFI_PAGE_SIZE);
        if (munmap(index->map, size) == -1) {
            perror("Error un-mmapping the file");
            exit(EXIT_FAILURE);
        }
        index->map = NULL;
    }
}

/**
 * Load a specific page into memory
 */
void bfi_load_mapped_page(bfi *index, int page) {

    if(page == index->current_page)  return;

    // check page is within current pages
    if(page >= index->total_pages) {
        //printf("Remapping file\n");
        bfi_release_map(index);
        // grow the file
        lseek(index->fp, BFI_HEADER + (BFI_PAGE_SIZE * (page+1)), SEEK_SET);
        if(write(index->fp, "", 1) == -1) {
            perror("Failed to extend file");
            exit(EXIT_FAILURE);
        }
        index->total_pages++;
    }

    int page_start = BFI_HEADER + (BFI_PAGE_SIZE * page);

    if(index->map == NULL) {
        index->map = mmap(0, BFI_HEADER + (index->total_pages * BFI_PAGE_SIZE),
                PROT_READ | PROT_WRITE, MAP_SHARED, index->fp, 0);
        if (index->map == MAP_FAILED) {
            close(index->fp);
            perror("Error mmapping the file");
            exit(EXIT_FAILURE);
        }
    }

    index->pks = (uint32_t *) &index->map[page_start];
    index->page = &(index->map[page_start + (BFI_PK_SIZE * BFI_RECORDS_PER_PAGE)]);


}

/**
 * Traverse the index looking for a pk.
 * If found the current page will be left at the correct page and the offset returned.
 * If not found the current page will be set to the last and -1 returned.
 */
int bfi_seek_pk(bfi * index, int pk) {
    int page, i;
    
    for(page=0; page<index->total_pages; page++) {
        bfi_load_mapped_page(index, page);
        for(i=0; i<BFI_RECORDS_PER_PAGE; i++)  {
            if(index->pks[i] == pk) {
                return i;
            }
        }
    }
    
    // pk wasn't found, return -1 and leave current page at end
    return -1;
}

/**
 * Insert or update a set of values for a pk.
 */
int bfi_insert(bfi * index, int pk, char * input[], int items) {
    int offset, i;
    char * data;

    bfi_generate(input, items, &data);

    offset = bfi_seek_pk(index, pk);
    
    if(offset == -1) { // wasn't found
        if(index->deleted > 0) {
            offset = bfi_seek_pk(index, 0);
            index->deleted--;
        } else {
            offset = index->records % BFI_RECORDS_PER_PAGE;
            index->records++;
        }
    }
    
    bfi_write_index(index, offset, pk, data);

    free(data);

    return 0;
}

/**
 * Append a set of values to the end of the index.
 * No checking for existance of pk so is possible to end up with inconsistent index.
 * Super efficient method for rebuilding index from scratch.
 */
int bfi_append(bfi * index, int pk, char * input[], int items) {
    int page, offset;
    char * data;

    page = index->records / BFI_RECORDS_PER_PAGE;
    offset = index->records % BFI_RECORDS_PER_PAGE;

    //printf("Page: %d, offset: %d\n", page, offset);
    bfi_load_mapped_page(index, page);

    bfi_generate(input, items, &data);

    bfi_write_index(index, offset, pk, data);

    free(data);

    index->records++;

    return 0;
}

/**
 * Delete a pk from the index.
 * Sets the pk and data to 0.
 */ 
int bfi_delete(bfi * index, int pk) {
    int offset;
    char * data;
    
    offset = bfi_seek_pk(index, pk);
    
    if(offset == -1) return -1;
    
    bfi_generate(NULL, 0, &data);
    bfi_write_index(index, offset, 0, data);
    
    free(data);
    index->deleted++;
    return 0;
}

int bfi_write_index(bfi *index, int offset, int pk, char * data) {
    int i;
    char * p;

    // write the PK
    index->pks[offset] = pk;

    // write the data
    p = index->page;
    p += offset;
    for(i=0; i<BLOOM_SIZE; i++) {
        *p = data[i];
        p += BFI_RECORDS_PER_PAGE;
    }

    return 0;
}

int bfi_lookup(bfi * index, char * input[], int items, uint32_t ** ptr) {
    int page, i, j, buf_size, count;
    char * data, matches[BFI_RECORDS_PER_PAGE];
    char * p_data, * p_index;
    uint32_t * result;

    count = 0;
    result = NULL;

    bfi_generate(input, items, &data);

    for(page=0; page<index->total_pages; page++) {
        bfi_load_mapped_page(index, page);
        //printf("PKS: ");
        //for(i=0; i<BFI_RECORDS_PER_PAGE; i++) printf("%4d ", index->pks[i]);
        //printf("\n");
        memset(matches, 1, BFI_RECORDS_PER_PAGE);

        p_data = data;
        p_index = index->page;

        for(i=0; i<BLOOM_SIZE; i++) {
            if(*p_data == 0) {
                p_index += BFI_RECORDS_PER_PAGE;
                //printf("-");
            } else {
                //printf("DATA: %02x\nINDEX: ", *p_data);
                for(j=0; j<BFI_RECORDS_PER_PAGE; j++) {
                    //printf("%02x ", *p_index);
                    if((*p_data & *p_index) != *p_data) {
                        matches[j] = 0;
                    }
                    p_index++;
                }
                //printf("\nMATCHES: ");
                //for(j=0; j<BFI_RECORDS_PER_PAGE; j++) printf("%d", (int)matches[j]);
                //printf("\n");
            }
            p_data++;
        }

        // output the result
        for(i=0; i<BFI_RECORDS_PER_PAGE; i++) {
            if(matches[i]) {
                if(count % 100 == 0) {
                    buf_size = ((count / 100) + 1) * 100;
                    result = realloc(result, BFI_PK_SIZE * buf_size);
                    if(result == NULL) {
                        perror("Failed to resize output");
                        exit(EXIT_FAILURE);
                    }
                }
                result[count++] = index->pks[i];
                //printf("%d ", index->pks[i]);
            }
        }

    }

    free(data);

    // shrink it back to actual size
    if(count) {
        result = realloc(result, BFI_PK_SIZE * count);
        if(result == NULL) {
            perror("Failed to resize output");
            exit(EXIT_FAILURE);
        }
    }

    *ptr = result;

    return count;
}
