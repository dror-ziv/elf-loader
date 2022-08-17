#include <stdio.h>
#include <stdlib.h>

int main(int argc, char** argv) {
    if (argc==3){
        printf("printing from elf_example!\n");
        int a = atoi(argv[1]);
        int b = atoi(argv[2]);
        return (a + b);
    }
    return -1;
}
