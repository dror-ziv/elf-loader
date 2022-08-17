all: elfloader elf_example

elfloader: main.c
	gcc -m32 -g -Wall -o elfloader main.c -ldl

elf_example: elf_example.c
	gcc -m32 -pie -fPIE -o elf_example elf_example.c
