# elf loader - Load and execute an ELF in user mode
a small elf loader. it can execute static and dynamically linked ELF EXEC.  
The ELF loader parses the ELF file, map it to memory , perform dynamic relocation where needed, searches for "main" function and executes it.  
more of a proof of concept than anything.  

## build
simply run

```
$ make
```

## run
currently can only handle 32-bit executable
compiled with 

```
$ gcc -m32 -pie -fPIE
```

## Load binaries
load is:

```
$ ./elfloader <elfbinary> <argv1> <argv2> <argv...>
```
