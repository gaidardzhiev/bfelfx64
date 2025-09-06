/*
 * Copyright (C) 2025 Ivan Gaydardzhiev
 * Licensed under the GPL-3.0-only
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct {
	uint8_t *data;
	size_t cap, len;
} Buf;

static void buf_init(Buf *b) {
	b->data = NULL;
	b->cap = b->len = 0;
}

static void buf_reserve(Buf *b, size_t need) {
	if (b->cap >= need) return;
	size_t ncap = b->cap ? b->cap * 2 : 512;
	while (ncap < need) ncap *= 2;
	uint8_t *p = (uint8_t*)realloc(b->data, ncap);
	if (!p) {
		perror("realloc");
		exit(1);
	}
	b->data = p;
	b->cap = ncap;
}

static void buf_append(Buf *b, const void *src, size_t n) {
	buf_reserve(b, b->len + n);
	memcpy(b->data + b->len, src, n);
	b->len += n;
}

static void buf_append_byte(Buf *b, uint8_t x) {
	buf_append(b, &x, 1);
}

static void buf_patch32_at(Buf *b, size_t off, uint32_t x) {
	if (off + 4 > b->len) {
		fprintf(stderr, "patch32 OOB\n");
		exit(1);
	}
	b->data[off + 0] = (uint8_t)(x);
	b->data[off + 1] = (uint8_t)(x >> 8);
	b->data[off + 2] = (uint8_t)(x >> 16);
	b->data[off + 3] = (uint8_t)(x >> 24);
}

static void le16(uint8_t *p, uint16_t x) {
	p[0] = x;
	p[1] = x >> 8;
}

static void le32(uint8_t *p, uint32_t x) {
	p[0] = x;
	p[1] = x >> 8;
	p[2] = x >> 16;
	p[3] = x >> 24;
}

static void le64(uint8_t *p, uint64_t x) {
	for (int i = 0; i < 8; i++) p[i] = (uint8_t)(x >> (8 * i));
}

typedef struct {
	Buf code;
} Code;

static void code_init(Code *c) {
	buf_init(&c->code);
}

static size_t code_pos(Code *c) {
	return c->code.len;
}

static void C8(Code *c, uint8_t x) {
	buf_append_byte(&c->code, x);
}

static void C32(Code *c, uint32_t x) {
	buf_append(&c->code, &x, 4);
}

static void emit_mov_r12_imm64(Code *c, uint64_t imm) {
	C8(c, 0x49);
	C8(c, 0xBC);
	le64((uint8_t*) &imm, imm);
	buf_append(&c->code, &imm, 8);
}

static void emit_initialize_r12(Code *c) {
	C8(c, 0x49);
	C8(c, 0xBC);
	for (int i = 0; i < 8; i++) C8(c, 0);
}

static void emit_inc_r12(Code *c) {
	C8(c, 0x49);
	C8(c, 0xFF);
	C8(c, 0xC4);
}

static void emit_dec_r12(Code *c) {
	C8(c, 0x49);
	C8(c, 0xFF);
	C8(c, 0xCC);
}

static void emit_inc_byte_ptr_r12(Code *c) {
	C8(c, 0x41);
	C8(c, 0xFE);
	C8(c, 0x04);
	C8(c, 0x24);
}

static void emit_dec_byte_ptr_r12(Code *c) {
	C8(c, 0x41);
	C8(c, 0xFE);
	C8(c, 0x0C);
	C8(c, 0x24);
}

static void emit_syscall_write(Code *c) {
	C8(c, 0x48);
	C8(c, 0xC7);
	C8(c, 0xC0);
	C8(c, 0x01);
	C8(c, 0x00);
	C8(c, 0x00);
	C8(c, 0x00);
	C8(c, 0x48);
	C8(c, 0xC7);
	C8(c, 0xC7);
	C8(c, 0x01);
	C8(c, 0x00);
	C8(c, 0x00);
	C8(c, 0x00);
	C8(c, 0x4C);
	C8(c, 0x89);
	C8(c, 0xE6);
	C8(c, 0x48);
	C8(c, 0xC7);
	C8(c, 0xC2);
	C8(c, 0x01);
	C8(c, 0x00);
	C8(c, 0x00);
	C8(c, 0x00);
	C8(c, 0x0F);
	C8(c, 0x05);
}

static void emit_syscall_read(Code *c) {
	C8(c, 0x48);
	C8(c, 0x31);
	C8(c, 0xC0);
	C8(c, 0x48);
	C8(c, 0x31);
	C8(c, 0xFF);
	C8(c, 0x4C);
	C8(c, 0x89);
	C8(c, 0xE6);
	C8(c, 0x48);
	C8(c, 0xC7);
	C8(c, 0xC2);
	C8(c, 0x01);
	C8(c, 0x00);
	C8(c, 0x00);
	C8(c, 0x00);
	C8(c, 0x0F);
	C8(c, 0x05);
}

static void emit_cmp_byte_ptr_r12_zero(Code *c) {
	C8(c, 0x41);
	C8(c, 0x80);
	C8(c, 0x3C);
	C8(c, 0x24);
	C8(c, 0x00);
}

static void emit_jne(Code *c, int8_t rel8) {
	C8(c, 0x75);
	C8(c, (uint8_t)rel8);
}

static void emit_je(Code *c, int8_t rel8) {
	C8(c, 0x74);
	C8(c, (uint8_t)rel8);
}

static size_t emit_jmp_rel32_placeholder(Code *c) {
	C8(c, 0xE9);
	size_t pos = code_pos(c);
	C32(c, 0);
	return pos;
}

static void patch_jmp_rel32(Code *c, size_t pos, size_t target) {
	int32_t rel = (int32_t)(target - (pos + 4));
	buf_patch32_at(&c->code, pos, (uint32_t)rel);
}

static const size_t TAPE_SIZE = 30000;

static void write_file(const char *path, Buf *file) {
	FILE *f = fopen(path, "wb");
	if (!f) {
		perror(path);
		exit(1);
	}
	if (fwrite(file->data, 1, file->len, f) != file->len) {
		perror("fwrite");
		exit(1);
	}
	fclose(f);
}

typedef struct {
	const char *src;
	size_t i, n;
	Code code;
} Ctx;

void compile(const char *src, const char *out_path) {
	Ctx ctx = {0};
	ctx.src = src;
	ctx.n = strlen(src);
	ctx.i = 0;
	code_init(&ctx.code);
	size_t ehdr_size = 64;
	size_t phdr_size = 56;
	size_t headers_size = ehdr_size + phdr_size * 2;
	Buf file;
	buf_init(&file);
	for(size_t i=0; i<headers_size; i++) buf_append_byte(&file, 0);
	emit_initialize_r12(&ctx.code);
	#define LOOPS 1024
	size_t loop_stack[LOOPS];
	size_t loop_stack_pos = 0;
	for (; ctx.i < ctx.n; ctx.i++) {
		char c = ctx.src[ctx.i];
		switch (c) {
		case '>':
			emit_inc_r12(&ctx.code);
			break;
		case '<':
			emit_dec_r12(&ctx.code);
			break;
		case '+':
			emit_inc_byte_ptr_r12(&ctx.code);
			break;
		case '-':
			emit_dec_byte_ptr_r12(&ctx.code);
			break;
		case '.':
			emit_syscall_write(&ctx.code);
			break;
		case ',':
			emit_syscall_read(&ctx.code);
			break;
		case '[': {
			emit_cmp_byte_ptr_r12_zero(&ctx.code);
			buf_append_byte(&ctx.code.code, 0x0F);
			buf_append_byte(&ctx.code.code, 0x84);
			size_t jmp_pos = code_pos(&ctx.code);
			C32(&ctx.code, 0);
			if(loop_stack_pos >= LOOPS) {
				fprintf(stderr,"too many nested loops\n");
				exit(1);
			}
			loop_stack[loop_stack_pos++] = jmp_pos;
			break;
		}
		case ']': {
			if(loop_stack_pos == 0) {
				fprintf(stderr, "loop end without matching start\n");
				exit(1);
			}
			size_t loop_start_jmp_pos = loop_stack[loop_stack_pos - 1];
			loop_stack_pos--;
			emit_cmp_byte_ptr_r12_zero(&ctx.code);
			buf_append_byte(&ctx.code.code, 0x0F);
			buf_append_byte(&ctx.code.code, 0x85);
			size_t jmp_back_pos = code_pos(&ctx.code);
			C32(&ctx.code, 0);
			size_t after_jne_pos = code_pos(&ctx.code);
			int32_t forward_rel = (int32_t)(after_jne_pos - (loop_start_jmp_pos + 4));
			buf_patch32_at(&ctx.code.code, loop_start_jmp_pos, (uint32_t)forward_rel);
			size_t loop_top_pos = loop_start_jmp_pos - (2 + 5);
int32_t back_rel = (int32_t)(loop_top_pos - (jmp_back_pos + 4));
buf_patch32_at(&ctx.code.code, jmp_back_pos, (uint32_t)back_rel);
			break;
		}
		default:
			break;
		}
	}
	if (loop_stack_pos != 0) {
		fprintf(stderr, "unclosed loops detected\n");
		exit(1);
	}
	C8(&ctx.code, 0x48);
	C8(&ctx.code, 0xC7);
	C8(&ctx.code, 0xC0);
	C8(&ctx.code, 60);
	C8(&ctx.code, 0x00);
	C8(&ctx.code, 0x00);
	C8(&ctx.code, 0x00);
	C8(&ctx.code, 0x48);
	C8(&ctx.code, 0x31);
	C8(&ctx.code, 0xFF);
	C8(&ctx.code, 0x0F);
	C8(&ctx.code, 0x05);
	size_t code_off = headers_size;
	size_t fsz = headers_size + ctx.code.code.len;
	size_t page_size = 0x1000;
	size_t tape_vaddr = 0x600000;
	uint8_t *tape_addr_pos = ctx.code.code.data + 2;
	le64(tape_addr_pos, tape_vaddr);
	buf_append(&file, ctx.code.code.data, ctx.code.code.len);
	uint8_t *E = file.data;
	E[0] = 0x7F;
	E[1] = 'E';
	E[2] = 'L';
	E[3] = 'F';
	E[4] = 2;//64bit
	E[5] = 1;//little endian
	E[6] = 1;//ver
	E[7] = 0;//system V ABI
	le16(E + 0x10, 2);//e_type = EXEC
	le16(E + 0x12, 0x3E);//e_machine = x86_64
	le32(E + 0x14, 1);//e_version
	le64(E + 0x18, 0x400000 + code_off);//e_entry program start (offset of code)
	le64(E + 0x20, 0x40);//e_phoff start program header table
	le64(E + 0x28, 0);//e_shoff no section header
	le32(E + 0x30, 0);//e_flags
	le16(E + 0x34, ehdr_size);//e_ehsize
	le16(E + 0x36, phdr_size);//e_phentsize
	le16(E + 0x38, 2);//e_phnum two program headers (text + bss)
	le16(E + 0x3A, 0);//e_shentsize
	le16(E + 0x3C, 0);//e_shnum
	le16(E + 0x3E, 0);//e_shstrndx
	//program header 1: text segment
	uint8_t *P1 = file.data + ehdr_size;
	le32(P1 + 0x00, 1);//p_type = PT_LOAD
	le32(P1 + 0x04, 5);//p_flags = RX
	le64(P1 + 0x08, 0);//p_offset in file
	le64(P1 + 0x10, 0x400000);//p_vaddr
	le64(P1 + 0x18, 0x400000);//p_paddr
	le64(P1 + 0x20, fsz);//p_filesz
	le64(P1 + 0x28, fsz);//p_memsz
	le64(P1 + 0x30, page_size);//p_align
	//program header 2: bss segment (tape)
	uint8_t *P2 = file.data + ehdr_size + phdr_size;
	le32(P2 + 0x00, 1);//p_type = PT_LOAD
	le32(P2 + 0x04, 6);//p_flags = RW
	le64(P2 + 0x08, 0);//p_offset = 0 since not in file (bss)
	le64(P2 + 0x10, tape_vaddr);//p_vaddr = tape address
	le64(P2 + 0x18, tape_vaddr);//p_paddr
	le64(P2 + 0x20,  0 );//p_filesz = 0 bytes in file (bss)
	le64(P2 + 0x28, TAPE_SIZE);//p_memsz = size of tape
	le64(P2 + 0x30, page_size);//p_align
	write_file(out_path, &file);
	free(ctx.code.code.data);
	free(file.data);
	fprintf(stderr, "wrote ELF64 x86_64 Brainf*ck program to %s\n", out_path);
}

char *rc(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		perror(path);
		exit(1);
	}
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *buf = malloc(n + 1);
	if (!buf) {
		perror("malloc");
		exit(1);
	}
	if (fread(buf, 1, n, f) != (size_t)n) {
		perror("fread");
		exit(1);
	}
	fclose(f);
	buf[n] = 0;
	return buf;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "usage: %s code.bf -o a.out\n", argv[0]);
		return 1;
	}
	const char *in = argv[1];
	const char *out = "a.out";
	for (int i = 2; i < argc; i++) {
		if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
			out = argv[++i];
		} else {
			fprintf(stderr, "unknown argument: %s\n", argv[i]);
			return 1;
		}
	}
	char *src = rc(in);
	compile(src, out);
	free(src);
	return 0;
}
