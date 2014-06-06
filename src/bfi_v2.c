#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "murmur.h"
#include "bfi.h"

#define BFI_VERSION 0x02

void bfi_release_map(bfi * index);

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

bfi * bfi_open(char * filename) {
    bfi * result;
    int i;
    
    //fprintf(stderr, "OPEN %s\n", filename);
    
    result = malloc(sizeof(bfi));
    if(result == NULL) {
        perror("Failed to allocate memory for resource");
        return NULL;
    }
    
    result->fp = open(filename, O_RDWR | O_CREAT, (mode_t)0600);
    if(!result->fp) {
        perror("Failed to open file");
        return NULL;
    }
    
    lseek(result->fp, 0, 0);
    i = read(result->fp, result, BFI_HEADER);
    if(i == 0) {
        //fprintf(stderr, "Creating new file\n");
        result->magic_number = BFI_MAGIC;
        result->version = BFI_VERSION;
        result->records = 0;
        write(result->fp, result, BFI_HEADER);
    }
    
    if(result->magic_number != BFI_MAGIC) {
        fprintf(stderr, "Bad magic number: 0x%0x\n", result->magic_number);
        return NULL;
    }
    
    if(result->version != BFI_VERSION) {
        fprintf(stderr, "Incorrect version - expected %d, got %d\n", BFI_VERSION, result->version);
        return NULL;
    }
    
    result->map = NULL;
    result->current_page = -1;
    result->total_pages = result->records ? (result->records / BFI_RECORDS_PER_PAGE) + 1 : 0;
    
    return result;
}

int bfi_sync(bfi * index) {
    
    if(index->map == NULL) return index->records;
    
    // write the current page to disk
    msync(&index->map[BFI_HEADER + (BFI_PAGE_SIZE * index->current_page)], BFI_PAGE_SIZE, MS_SYNC);
    
    memcpy(index->map, index, BFI_HEADER);
    msync(index->map, BFI_HEADER, MS_SYNC);
    
    return index->records;
}

void bfi_close(bfi * index) {
    bfi_sync(index);
    bfi_release_map(index);
    
    close(index->fp);
    
    free(index);
}

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

int bfi_index(bfi * index, int pk, char * input[], int items) {
    int page, offset, i;
    char * p, * data;
    
    page = index->records / BFI_RECORDS_PER_PAGE;
    offset = index->records % BFI_RECORDS_PER_PAGE;
    
    //printf("Page: %d, offset: %d\n", page, offset);
    bfi_load_mapped_page(index, page);
    
    bfi_generate(input, items, &data);
    
    // write the PK
    index->pks[offset] = pk;
    
    // write the data
    p = index->page;
    p += offset;
    for(i=0; i<BLOOM_SIZE; i++) {
        *p = data[i];
        p += BFI_RECORDS_PER_PAGE;
    }
    
    free(data);
    
    index->records++;
    
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
