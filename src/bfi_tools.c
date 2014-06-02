#include <stdio.h>
#include "bfi.h"

int usage() {
    fprintf(stderr, "Usage: bfi index <pk> <value> [<value> ...]\n");
    fprintf(stderr, "       bfi index (read from stdin)");
    fprintf(stderr, "       bfi lookup <value> [<value> ...]\n");
    return 1;
}

int main(int argc, char *argv[]) {
    bfi *index;
    int pk;
    
    if(argc < 2) return usage();
    
    index = bfi_open("foo.bfi");
    
    if(strcmp(argv[1], "index") == 0) {
        
        if(argc < 3) { // read from stdin
            printf("Indexed %d items\n", bfi_index_stdin(index, 0));
        } else { // single input
            sscanf(argv[2], "%d", &pk);
            bfi_index(index, pk, &argv[3], argc-3);
        }
        
    } else if(strcmp(argv[1], "lookup") == 0) {
        if(argc < 3) return usage();
        
        bfi_lookup(index, &argv[2], argc-2);
    } else {
        return usage();
    }
    
    bfi_close(index);
    return 0;
}