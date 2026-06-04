#include <iostream>
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char **argv ) {
    // flags:
    // -h - "prints this help document"
    // -T # (number of CPU threads to dedicate) -- default 1 (so I can benchmark improvement with 1 vs 2 vs 4 vs 8 vs 16 cores)
    // -R # resolution (format is "WxH; e.g. 1920x1080")

    if(argc == 1) {
        printf("Usage: %s -T # -R 1920x1080");
    }






}
