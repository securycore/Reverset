#include "read_elf.h"

void elf_read_file(FILE * f, rfile * file)
{
	file->bits = fgetc(f);
	rewind(f);
	if (file->bits == ELFCLASS32) {
		elf_read32(f, file);
	} else if (file->bits == ELFCLASS64) {
		elf_read64(f, file);
	}
	//Copy the entire file into rfile
	fseek(f, 0, SEEK_END);
	int len = ftell(f);
	rewind(f);
	file->raw_file = malloc(len);
	fread(file->raw_file, len, 1, f);
}

void elf_read32(FILE * f, rfile * file)
{
	Elf32_Ehdr header;
	fread(&header, ALIGNED_SIZEOF(Elf32_Ehdr), 1, f);
	//Set rfiles architecture, entry_point and number of sections
	switch (header.e_machine) {
		case EM_ARM:
			file->arch = r_arm;
			break;
		case EM_X86_64:
			file->arch = r_x86_64;
			break;
		case EM_386:
			file->arch = r_x86;
			break;
		default:
			printf("Architecture unsupported\n");
			return;
			break;
	}
	file->entry_point = header.e_entry;
	file->num_sections = header.e_shnum;
	file->sections = malloc(sizeof(rsection) * file->num_sections);
	memset(file->sections, 0, sizeof(rsection) * file->num_sections);
	//Seek to section header offset to read the section entries
	fseek(f, header.e_shoff, SEEK_SET);

	Elf32_Shdr *sections = malloc(sizeof(Elf32_Shdr) * header.e_shnum);
	fread(sections, sizeof(Elf32_Shdr)*header.e_shnum, 1, f);
	int names_offset = -1;
	//If there is a section name string table then go to it
	if (header.e_shstrndx != SHN_UNDEF) {
		names_offset = sections[header.e_shstrndx].sh_offset;
	}
	for (int i = 0; i < header.e_shnum; i++) {
		rsection r;
		r.name = NULL;
		r.size = sections[i].sh_size;
		r.start32 = sections[i].sh_addr;
		r.type = r_notype;
		if (sections[i].sh_type == SHT_PROGBITS) {
			r.type = r_programdefined;
		} else if (sections[i].sh_type == SHT_SYMTAB) {
			r.type = r_symboltab;
		} else if (sections[i].sh_type == SHT_STRTAB) {
			r.type = r_stringtab;
		} else if (sections[i].sh_type != SHT_NULL) {
			r.type = r_other;
		}
		//Copy the raw bytes into the rsection
		fseek(f, sections[i].sh_offset, SEEK_SET);
		r.raw = malloc(r.size);
		fread(r.raw, r.size, 1, f);
		//If there is a section name table read the name
		if (names_offset != -1) {
			fseek(f, names_offset+sections[i].sh_name, SEEK_SET);
			char c = fgetc(f);
			int iter = 0;
			char buf[256];
			while (c && iter < 256) {
				buf[iter++] = c;
				c = fgetc(f);
			}
			buf[iter] = 0;
			r.name = strdup(buf);
		}
		file->sections[i] = r;
	}
	int strtabidx = -1;
	//Find STRTAB
	for (int i = 0; i < header.e_shnum; i++) {
		if (!strcmp(file->sections[i].name, ".strtab")) {
			strtabidx = i;
			break;
		}
	}
	rsection r;
	if (strtabidx != -1) r = file->sections[strtabidx];
	file->num_symbols = 0;
	//Look for sections of type SHT_SYMTAB and find symbols
	for (int i = 0; i < header.e_shnum; i++) {
		if (sections[i].sh_type == SHT_SYMTAB) {
			int num_symbols = sections[i].sh_size/sizeof(Elf32_Sym);
			Elf32_Sym * symbols = (Elf32_Sym*)(file->sections[i].raw);
			for (int j = 0; j < num_symbols; j++) {
				if (strtabidx != -1) {
					int iter = 0;
					char c = r.raw[symbols[j].st_name];
					char buf[256];
					while (c && iter < 256) {
						buf[iter++] = c;
						c = r.raw[symbols[j].st_name + iter];
					}
					buf[iter] = 0;
					//Do not use unknown or no type symbols
					if (buf[0] != 0 && (elft_to_rsymt(ELF32_ST_TYPE(symbols[j].st_info)) != R_NONE)) {
						//Only make rsymbol if the string is not NULL
						rsymbol rsym;
						rsym.name = strdup(buf);
						rsym.type = elft_to_rsymt(ELF32_ST_TYPE(symbols[j].st_info));
						//rsym.type = symbols[j].st_type;
						rsym.addr32 = symbols[j].st_value;

						file->num_symbols++;
						if (file->num_symbols == 1) {
							file->symbols = malloc(sizeof(rsymbol));
						} else {
							file->symbols = realloc(file->symbols, sizeof(rsymbol) * file->num_symbols);
						}
						file->symbols[file->num_symbols-1] = rsym;
					}
				}

			}

		}
	}

	free(sections);
}

void elf_read64(FILE * f, rfile * file)
{
	Elf64_Ehdr header;
	fread(&header, ALIGNED_SIZEOF(Elf64_Ehdr), 1, f);
	//Set rfiles architecture, entry_point and number of sections
	switch (header.e_machine) {
		case EM_ARM:
			file->arch = r_arm;
			break;
		case EM_X86_64:
			file->arch = r_x86_64;
			break;
		case EM_386:
			file->arch = r_x86;
			break;
		default:
			printf("Architecture unsupported\n");
			return;
			break;
	}
	file->entry_point = header.e_entry;
	file->num_sections = header.e_shnum;
	file->sections = malloc(sizeof(rsection) * file->num_sections);
	memset(file->sections, 0, sizeof(rsection) * file->num_sections);
	//Seek to section header offset to read the section entries
	fseek(f, header.e_shoff, SEEK_SET);

	Elf64_Shdr *sections = malloc(sizeof(Elf64_Shdr) * header.e_shnum);
	fread(sections, sizeof(Elf64_Shdr)*header.e_shnum, 1, f);
	int names_offset = -1;
	//If there is a section name string table then go to it
	if (header.e_shstrndx != SHN_UNDEF) {
		names_offset = sections[header.e_shstrndx].sh_offset;
	}
	for (int i = 0; i < header.e_shnum; i++) {
		rsection r;
		r.name = NULL;
		r.size = sections[i].sh_size;
		r.start32 = sections[i].sh_addr;
		r.type = r_notype;
		if (sections[i].sh_type == SHT_PROGBITS) {
			
			r.type = r_programdefined;
		} else if (sections[i].sh_type == SHT_SYMTAB) {
			r.type = r_symboltab;
		} else if (sections[i].sh_type == SHT_STRTAB) {
			r.type = r_stringtab;
		} else if (sections[i].sh_type != SHT_NULL) {
			r.type = r_other;
		}
		//Copy the raw bytes into the rsection
		fseek(f, sections[i].sh_offset, SEEK_SET);
		r.raw = malloc(r.size);
		fread(r.raw, r.size, 1, f);
		//If there is a section name table read the name
		if (names_offset != -1) {
			fseek(f, names_offset+sections[i].sh_name, SEEK_SET);
			char c = fgetc(f);
			int iter = 0;
			char buf[256];
			while (c && iter < 256) {
				buf[iter++] = c;
				c = fgetc(f);
			}
			buf[iter] = 0;
			r.name = strdup(buf);
		}
		file->sections[i] = r;
	}
	int strtabidx = -1;
	//Find STRTAB
	for (int i = 0; i < header.e_shnum; i++) {
		if (!strcmp(file->sections[i].name, ".strtab")) {
			strtabidx = i;
			break;
		}
	}
	rsection r;
	if (strtabidx != -1) r = file->sections[strtabidx];
	file->num_symbols = 0;
	//Look for sections of type SHT_SYMTAB and find symbols
	for (int i = 0; i < header.e_shnum; i++) {
		if (sections[i].sh_type == SHT_SYMTAB) {
			int num_symbols = sections[i].sh_size/sizeof(Elf64_Sym);
			Elf64_Sym * symbols = (Elf64_Sym*)(file->sections[i].raw);
			for (int j = 0; j < num_symbols; j++) {
				if (strtabidx != -1) {
					int iter = 0;
					char c = r.raw[symbols[j].st_name];
					char buf[256];
					while (c && iter < 256) {
						buf[iter++] = c;
						c = r.raw[symbols[j].st_name + iter];
					}
					buf[iter] = 0;
					//Do not use unknown or no type symbols
					if (buf[0] != 0 && (elft_to_rsymt(ELF64_ST_TYPE(symbols[j].st_info)) != R_NONE)) {
						//Only make rsymbol if the string is not NULL
						rsymbol rsym;
						rsym.name = strdup(buf);
						rsym.type = elft_to_rsymt(ELF64_ST_TYPE(symbols[j].st_info));
						//rsym.type = symbols[j].st_type;
						rsym.addr32 = symbols[j].st_value;

						file->num_symbols++;
						if (file->num_symbols == 1) {
							file->symbols = malloc(sizeof(rsymbol));
						} else {
							file->symbols = realloc(file->symbols, sizeof(rsymbol) * file->num_symbols);
						}
						file->symbols[file->num_symbols-1] = rsym;
					}
				}

			}

		}
	}


	free(sections);
}

int elft_to_rsymt(int elfsymt)
{
	switch (elfsymt) {
		case STT_NOTYPE:
			break;
		case STT_OBJECT:
			return R_OBJECT;
			break;
		case STT_FUNC:
			return R_FUNC;
			break;
		default:
			return R_NONE;
			break;
	}
}

int check_elf(FILE * f)
{
	uint32_t elfm;
	fread(&elfm, 4, 1, f);
	return elfm == *((uint32_t*)elf_magic);
}