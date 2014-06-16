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
void bfi_generate(char * input[], int items, char ** ptr, int size) {
  // use 128 bytes and 12 sectors
  uint32_t hash;
  uint32_t i, offset, pos;
  char * bloom;

  //for(i=0; i<items; i++) printf("GENERATE: %s\n", input[i]);

  bloom = malloc(size);
  if(bloom == NULL) {
    perror("Failed to allocate memory for bloom filter");
    exit(EXIT_FAILURE);
  }
  memset(bloom, 0, size);

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
    result->slots = 0;
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
  result->total_pages = result->slots ? (result->slots / BFI_RECORDS_PER_PAGE) + 1 : 0;
  result->page_size = result->format * BFI_RECORDS_PER_PAGE;

  return result;
}

/**
 * Flush in memory changes to disk
 */
uint32_t bfi_sync(bfi * index) {

  if(index->map == NULL) return index->slots;

  // write the current page to disk
  msync(&index->map[BFI_HEADER + (index->page_size * index->current_page)], index->page_size, MS_SYNC);

  memcpy(index->map, index, BFI_HEADER);
  msync(index->map, BFI_HEADER, MS_SYNC);

  return index->slots;
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
    int size = BFI_HEADER + (index->total_pages * index->page_size);
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

  //printf("Load page %d\n", page);
  // check page is within current pages
  if(page >= index->total_pages) {
    //printf("Remapping file\n");
    bfi_release_map(index);
    // grow the file
    lseek(index->fp, BFI_HEADER + (index->page_size * (page+1)), SEEK_SET);
    if(write(index->fp, "", 1) == -1) {
      perror("Failed to extend file");
      exit(EXIT_FAILURE);
    }
    index->total_pages++;
  }

  int page_start = BFI_HEADER + (index->page_size * page);

  if(index->map == NULL) {
    index->map = mmap(0, BFI_HEADER + (index->total_pages * index->page_size),
		      PROT_READ | PROT_WRITE, MAP_SHARED, index->fp, 0);
    if (index->map == MAP_FAILED) {
      close(index->fp);
      perror("Error mmapping the file");
      exit(EXIT_FAILURE);
    }
  }

  index->page = &index->map[page_start];
  index->current_page = page;
}

/**
 * Write the specified slot.
 */
uint32_t bfi_write(bfi * index, uint32_t slot, char * input[], int items) {
  int page, offset, i;
  char * data, * p;

  page = slot / BFI_RECORDS_PER_PAGE;
  offset = slot % BFI_RECORDS_PER_PAGE;

  bfi_generate(input, items, &data, index->format);

  bfi_load_mapped_page(index, page);

  p = index->page;
  p += offset;
  for(i=0;i<index->format; i++) {
    *p = data[i];
    p += BFI_RECORDS_PER_PAGE;
  }

  free(data);

  return slot;
}

/**
 * Append data to the end of the structure and return the slot.
 */
uint32_t bfi_append(bfi * index, char * input[], int items) {
  return bfi_write(index, index->slots++, input, items);
}

/**
 * Lookup the specified items and return array of slots. 
 */
int bfi_lookup(bfi * index, char * input[], int items, uint32_t ** ptr) {
  int page, i, j, buf_size, slot_size, count;
  char * data, matches[BFI_RECORDS_PER_PAGE];
  char * p_data, * p_index;
  uint32_t * result;

  slot_size = sizeof(uint32_t);

  count = 0;
  result = NULL;

  bfi_generate(input, items, &data, index->format);

  //printf("Total pages: %d\n", index->total_pages);
  for(page=0; page<index->total_pages; page++) {
    bfi_load_mapped_page(index, page);

    memset(matches, 1, BFI_RECORDS_PER_PAGE);

    p_data = data;
    p_index = index->page;

    for(i=0; i<index->format; i++) {
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
	  result = realloc(result, slot_size * buf_size);
	  if(result == NULL) {
	    perror("Failed to resize output");
	    exit(EXIT_FAILURE);
	  }
	}
	result[count++] = page * BFI_RECORDS_PER_PAGE + i;
      }
    }

  }

  free(data);

  // shrink it back to actual size
  if(count) {
    result = realloc(result, slot_size * count);
    if(result == NULL) {
      perror("Failed to resize output");
      exit(EXIT_FAILURE);
    }
  }

  *ptr = result;

  return count;
}
