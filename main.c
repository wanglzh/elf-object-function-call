#include <stdio.h> // calling printf, fread
#include <sys/mman.h> // calling mprotect
#include <stdlib.h> // calling aligned_alloc
#include <unistd.h> // calling sysconf
#include <string.h> // calling memcpy 
#include <elf.h> //parse file in data structure as "Elf64_Ehdr" for file, "Elf64_Shdr" for sections

#include "component.c" // component implementation

#define USE_ELF

int main(int argc, char** argv)
{
	printf("hello PIC test!\n");

#ifdef USE_SRC
	int ret = entry_point();
	printf("The result of calling the component is: %d\n", ret);
#endif 

	/*********************************************
	 * testing function calling on heap
	 **********************************************/
	size_t page_size = sysconf(_SC_PAGESIZE);
	printf("the page size is: %lu\n", page_size);

	//give execution access to the specified memory region
	size_t memory_size = page_size * 2;
	void* buffer = aligned_alloc(page_size, memory_size);
	mprotect(buffer, memory_size, PROT_READ|PROT_WRITE|PROT_EXEC);

#ifdef USE_ELF
	//reading object file's text section to another file: "objcopy --dump-section .text=com.bin com.o"
	//instead of moving the .text section out, now reading the whole object file to memory
	printf("---using function from elf file.\n");
	FILE* f = fopen("./com.o", "rb");
	size_t size = fread(buffer, 1, memory_size, f);
	printf("read size: %lu\n", size);
#else
	//copy code from .text to heap
	printf("---using function from .text memory.");
	memcpy(buffer, entry_point, 20);
#endif

	/*parse Elf*/
	Elf64_Ehdr elf_hdr;
	memcpy(&elf_hdr, buffer, sizeof(elf_hdr));

	//get string table for section name and symbol name
	size_t shstrtab_offset = elf_hdr.e_shoff + elf_hdr.e_shstrndx * elf_hdr.e_shentsize;
	char* shstrtab = buffer + ((Elf64_Shdr*)(buffer + shstrtab_offset))->sh_offset;
        char* strtab = NULL;
	for(size_t i = 0; i < elf_hdr.e_shnum; i++){

		size_t offset = elf_hdr.e_shoff + i * elf_hdr.e_shentsize;
		Elf64_Shdr shdr;
		memcpy(&shdr, buffer + offset, sizeof(shdr));

		if(shdr.sh_type == SHT_STRTAB && offset != shstrtab_offset){
			strtab = buffer + shdr.sh_offset;
			break;
		}		
	}

	//get .text offset in elf file
	size_t text_offset = 0;
	size_t symtab_off = 0;//along get the symbol table info: offset in elf file and size
	size_t symtab_size = 0;
	for(size_t i = 0; i < elf_hdr.e_shnum; i++){
		size_t offset = elf_hdr.e_shoff + i * elf_hdr.e_shentsize;
		Elf64_Shdr shdr;
		memcpy(&shdr, buffer + offset, sizeof(shdr));
		
		if(strcmp(shstrtab + shdr.sh_name, ".text") == 0){
			text_offset = shdr.sh_offset;
		}

		if(shdr.sh_type == SHT_SYMTAB){
			symtab_off = shdr.sh_offset;
			symtab_size = shdr.sh_size;
		}
	}

	//get symbol value (relative address in .text) from symtab
	size_t f1_addr = 0;
	size_t f2_addr = 0;
	size_t ssize = sizeof(Elf64_Sym);	
	for(size_t j = 0; j * ssize < symtab_size; j++){
		Elf64_Sym sym;
		size_t offset = symtab_off + j * ssize;
		memcpy(&sym, buffer+offset, sizeof(sym));
		
		char* sym_name = strtab+sym.st_name;
		if(strcmp(sym_name, "entry_point") == 0){
			f1_addr = sym.st_value;
			
		}else if(strcmp(sym_name, "my_test") == 0){
			f2_addr = sym.st_value;
		}
	}

	//cast to call
	typedef int (*PF)();
	PF pf = buffer + text_offset + f1_addr;
	PF pf2 = buffer + text_offset + f2_addr;
	
	int ret = pf();
	printf("the result of the first componet call is: %d \n", ret);

	ret = pf2();
	printf("the result of the second component call is: %d \n", ret);

	return 0;

}
