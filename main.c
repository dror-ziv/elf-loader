#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <elf.h>
#include <dlfcn.h>
#include <assert.h>


/* structs and constants, assuming little endian, 32 bit file, 86x64 archtetura: */

#define ELF_MAGIC 0x464C457FU  /* "\x7FELF" in little endian */

#define PT_LOAD           1
#define PT_GNU_STACK 0x6474e551
#define ELF_PROG_FLAG_EXEC      1
#define ELF_PROG_FLAG_WRITE     2
#define ELF_PROG_FLAG_READ      4

typedef Elf32_Ehdr Elf_Ehdr;
typedef Elf32_Phdr Elf_Phdr;
typedef Elf32_Sym Elf_Sym;
typedef Elf32_Shdr Elf_Shdr;
typedef Elf32_Rel Elf_Rel;
typedef Elf32_Word Elf_Word;
#define ELF_R_SYM(x) ELF32_R_SYM(x)
#define ELF_R_TYPE(x) ELF32_R_TYPE(x)

/* global handle var for loading "libc.so.6" dynamically */
static void *handle = NULL;


int find_global_symbol_table(Elf_Ehdr* hdr, Elf_Shdr* shdr) {
    for (int i = 0; i < hdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_DYNSYM) {
            return i;
            break;
        }
    }
    return -1;
}


int find_symbol_table(Elf_Ehdr* hdr, Elf_Shdr* shdr) {
    for (int i = 0; i < hdr->e_shnum; i++) {
        if (shdr[i].sh_type == SHT_SYMTAB) {
            return i;
            break;
        }
    }
    return -1;
}


void* find_sym(const char* name, Elf_Shdr* shdr, Elf_Shdr* shdr_sym, const char* src, char* dst) {
    Elf_Sym* syms = (Elf_Sym*)(src + shdr_sym->sh_offset);
    const char* strings = src + shdr[shdr_sym->sh_link].sh_offset;
    for (int i = 0; i < shdr_sym->sh_size / sizeof(Elf_Sym); i += 1) {
        if (strcmp(name, strings + syms[i].st_name) == 0) {
            return dst + syms[i].st_value;
        }
    }
    return NULL;
}


void relocate(Elf_Shdr* shdr, const Elf_Sym* syms, const char* strings, const char* src, char* dst) {
    Elf_Rel* rel = (Elf_Rel*)(src + shdr->sh_offset);

    for(int j = 0; j < shdr->sh_size / sizeof(Elf_Rel); j += 1) {
        const char* sym = strings + syms[ELF_R_SYM(rel[j].r_info)].st_name;
        switch (ELF_R_TYPE(rel[j].r_info)) {
            case R_386_JMP_SLOT:
            case R_386_GLOB_DAT:
                *(Elf_Word*)(dst + rel[j].r_offset) = (Elf_Word)dlsym(handle, sym);
                break;
        }
    }
}


int validate_ehdr_magic(Elf_Ehdr *ehdr) {
    int magic = ELF_MAGIC;
     if(memcmp(&ehdr->e_ident, &magic, sizeof(magic))==0) {
        printf("magic number is good !\n");
        return 0;
      } else {
          return 1;
      }
}


void elf_map(char *elf_file,int fsize, size_t *base_destnation_addr) {
    Elf_Ehdr * ehdr = (Elf_Ehdr *) elf_file;
    Elf_Phdr * phdr = (Elf_Phdr *) (elf_file + ehdr->e_phoff);
    char *base_addr = NULL;

    /* clear place for memory */
    base_addr = mmap(NULL, fsize, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE | MAP_ANON , 0, 0);
    memset(base_addr, 0x0, fsize);
    *base_destnation_addr = (size_t)base_addr;

    /* itirate over every program header */
    for(int i = 0; i<ehdr->e_phnum; i++) {
        int flags = 0;        
        if (phdr[i].p_type!=PT_LOAD || !phdr[i].p_filesz) {
            continue;
        }

        /* calculate start address rounded to page size */
        void *map_start_to = (void *) phdr[i].p_vaddr + (size_t)base_addr;
        void *map_start_from = (void *) elf_file + phdr[i].p_offset;
        
        /* map the file in ram memmory */
        memmove(map_start_to, map_start_from, phdr[i].p_filesz);
        
        /* zero bss */
        if( phdr[i].p_filesz < phdr[i].p_memsz) {
         memset((void *)(base_addr + phdr[i].p_vaddr + phdr[i].p_filesz), 0, phdr[i].p_memsz - phdr[i].p_filesz);
        }

        /* set flags on mapped segments */
        if(phdr[i].p_flags & ELF_PROG_FLAG_READ) {
            flags =  PROT_READ;
         }
         if(phdr[i].p_flags & ELF_PROG_FLAG_WRITE) {
            flags |= PROT_WRITE;
         }
         if(phdr[i].p_flags & ELF_PROG_FLAG_EXEC) {
            flags |= PROT_EXEC;
        }
        mprotect((unsigned char *) (map_start_to), phdr[i].p_filesz, PROT_READ|PROT_WRITE|PROT_EXEC);
    }
}


int elf_load_file(void *file, int argc, char * argv[], char *envp[]) {
    size_t elf_base = 0, elf_entry = 0;
    int envc = 0;

    /* read whole file to memory */
    fseek(file, 0, SEEK_END);
    int fsize = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *elf_file = malloc(fsize);
    fread(elf_file, fsize, 1, file);

    /* validate magic number */
    Elf_Ehdr* ehdr = (Elf_Ehdr *) elf_file;
    if (validate_ehdr_magic(ehdr)) {
        return 0;
    }
    
    /* set argv, count envp */
    argc--;
    for (int i = 0;i<argc;i++) {
        argv[i] = argv[i+1];
    }
    while(envp[envc]) {
        envc++;
    }

    /* map segments */
    elf_map(elf_file,fsize, &elf_base);

    /* relocate dynamyc symbols: */
    Elf_Shdr * shdr = NULL;
    shdr = (Elf_Shdr *) (elf_file + ehdr->e_shoff);
    int global_symbol_table_index = find_global_symbol_table(ehdr, shdr);
    Elf_Sym* global_syms = (Elf_Sym*)(elf_file + shdr[global_symbol_table_index].sh_offset);
    char* global_strings = elf_file + shdr[shdr[global_symbol_table_index].sh_link].sh_offset;
    for (int i = 0; i < ehdr->e_shnum; ++i) {
        if (shdr[i].sh_type == SHT_REL)
        {
            relocate(shdr + i, global_syms, global_strings, elf_file, ((char *)elf_base));

        }
    }

    /* find main funcyion sym address: */
    int symbol_table_index = find_symbol_table(ehdr, shdr);
    elf_entry = (size_t)find_sym("main", shdr, shdr + symbol_table_index, elf_file, (char *)elf_base);

    int (*func_ptr)(int, char **, char**);
    func_ptr = (int (*)(int,  char **, char **))elf_entry;
    int sum = func_ptr(argc, argv, envp);
    printf("%d\n", sum);
    return 1;
}


int main(int argc, char*argv[], char *envp[]) {
    if (argc < 2) {
        printf("format must be: 'elfload <bin> <arg1> <arg2>..\n");
        return 1;
    }
    FILE* file = fopen(argv[1], "rb");
    if (!file) {
        printf("file could not open\n");
        return 1;
    } else {
        handle = dlopen("libc.so.6", RTLD_NOW);
        if(!elf_load_file(file, argc, argv, envp)) {
            printf("ERROR. file could not execute.\n");
        }
    fclose(file);
    return 0;
    }
}
