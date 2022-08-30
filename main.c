/*
    rgimad
    BSD2-clause license
    Based on:
    https://github.com/AntKrotov/oberon-07-compiler
*/

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define REG_EAX 0
#define REG_ECX 1
#define REG_EDX 2
#define REG_EBX 3
#define REG_ESP 4
#define REG_EBP 5
#define REG_ESI 6
#define REG_EDI 7

#define TEST(action, resbuf, resbuf_len, expected) {outbuf_clear(); \
    action; \
    assert(resbuf_len == sizeof(expected)); \
    assert(memcmp(resbuf, expected, resbuf_len) == 0); \
}

#define TEST_VERBOSE(action, resbuf, resbuf_len, expected) {outbuf_clear(); \
    action; \
    print_bytes(resbuf, resbuf_len); \
    assert(resbuf_len == sizeof(expected)); \
    assert(memcmp(resbuf, expected, resbuf_len) == 0); \
}

uint8_t outbuf[1024];
size_t outbuf_len = 0;

void outbuf_clear() {
    memset(outbuf, 0, outbuf_len);
    outbuf_len = 0;
}

void print_bytes(const uint8_t *buf, size_t len) {
    printf("{ ");
    for (size_t i = 0; i < len; i++) {
        printf("0x%02X", buf[i]);
        if (i < len - 1) {
            printf(", ");
        }
    }
    printf(" }\n");
}

void out_byte(uint8_t a) {
    // printf("%02X ", a);
    outbuf[outbuf_len++] = a;
}

void out_byte2(uint8_t a, uint8_t b) {
    out_byte(a);
    out_byte(b);
}

void out_int(int a) {
    out_byte(a & 0xFF);
    out_byte((a >> 8) & 0xFF);
    out_byte((a >> 16) & 0xFF);
    out_byte((a >> 24) & 0xFF);
}

bool is_byte(int n) {
    return (-128 <= n) && (n <= 127);
}

void out_int_or_byte(int n) {
    if (is_byte(n)) {
        out_byte(n & 0xFF);
    } else {
        out_int(n);
    }
}

void oprr(uint8_t op, int reg1, int reg2) {
    out_byte2(op, 0x0C0 + 8*reg2 + reg1);
}

// mov reg1, reg2
void mov(int reg1, int reg2) {
    oprr(0x89, reg1, reg2);
}

// xchg reg1, reg2
void xchg(int reg1, int reg2) {
    if (reg1 == REG_EAX || reg2 == REG_EAX) {
        out_byte(0x90 + reg1 + reg2);
    } else {
        oprr(0x87, reg1, reg2);
    }
}

// pop reg
void pop(int reg) {
    out_byte(0x58 + reg);
}

// push reg
void push(int reg) {
    out_byte(0x50 + reg);
}

// xor reg1, reg2
void xor(int reg1, int reg2) {
    oprr(0x31, reg1, reg2);
}

// mov reg, n
void movrc_raw(int reg, int n) {
    out_byte(0xB8 + reg);
    out_int(n);
}

// optimized mov reg, n
void movrc(int reg, int n) {
    if (n == 0) {
        xor(reg, reg);
    } else {
        movrc_raw(reg, n);
    }
}

// push n
void pushc(int n) {
    out_byte(0x68 + (is_byte(n) ? 2 : 0));
    out_int_or_byte(n);
}

// test reg, reg
void test(int reg) {
    out_byte2(0x85, 0xC0 + reg*9);
}

// neg reg
void neg(int reg) {
    out_byte2(0xF7, 0xD8 + reg);
}

// not reg
void not(int reg) {
    out_byte2(0xF7, 0xD0 + reg);
}

// add reg1, reg2
void add(int reg1, int reg2) {
    oprr(0x1, reg1, reg2);
}

void oprc(int op, int reg, int n) {
    if (reg == REG_EAX && !is_byte(n)) {
        switch (op) {
            case 0xC0:
                op = 0x05; // add
                break;
            case 0xE8:
                op = 0x2D; // sub
                break;
            case 0xF8:
                op = 0x3D; // cmp
                break;
            case 0xE0:
                op = 0x25; // and
                break;
            case 0xC8:
                op = 0x0D; // or
                break;
            case 0xF0:
                op = 0x35; // xor
                break;   
        }
        out_byte(op);
        out_int(n);
    } else {
        out_byte2(0x81 + (is_byte(n) ? 2 : 0), op + reg % 8);
        out_int_or_byte(n);
    }
}

// and reg, n
void andrc(int reg, int n) {
    oprc(0xE0, reg, n);
}

// or reg, n
void orrc(int reg, int n) {
    oprc(0xC8, reg, n);
}

// xor reg, n
void xorrc(int reg, int n) {
    oprc(0xF0, reg, n);
}

// add reg, n
void addrc(int reg, int n) {
    oprc(0xC0, reg, n);
}

// sub reg, n
void subrc(int reg, int n) {
    oprc(0xE8, reg, n);
}


// ....

void _movrm(int reg1, int reg2, int offs, int size, bool mr) {
    if (size == 16) {
        out_byte(0x66);
    }
    if (reg1 >= 8 || reg2 >= 8 || size == 64) {
        out_byte(0x40 + reg2/8 + 4*(reg1/8) + (size == 64 ? 8 : 0));
    }
    out_byte(0x8B - (mr ? 2 : 0) - (size == 8));
    uint8_t b;
    if (offs == 0 && reg2 != REG_EBP) {
        b = 0;
    } else {
        b = 0x40 +  (!is_byte(offs) ? 0x40 : 0);
    }
    out_byte(b + (reg1 % 8)*8 + reg2 % 8);
    if (reg2 == REG_ESP) {
        out_byte(0x24);
    }
    if (b != 0) {
        out_int_or_byte(offs);
    }
}

// mov dword [reg1 + offs], reg2
void movmr(int reg1, int offs, int reg2) {
    _movrm(reg2, reg1, offs, 32, true);
}

// mov reg1, dword [reg2 + offs]
void movrm(int reg1, int offs, int reg2) {
    _movrm(reg1, reg2, offs, 32, false);
}


void run_tests() {
    TEST(mov(REG_EAX, REG_EBX), outbuf, outbuf_len, ((uint8_t[]){ 0x89, 0xD8 }));
    TEST(mov(REG_ESI, REG_ECX), outbuf, outbuf_len, ((uint8_t[]){ 0x89, 0xCE }));
    TEST(push(REG_EDI), outbuf, outbuf_len, ((uint8_t[]){ 0x57 }));
    TEST(pop(REG_EBX), outbuf, outbuf_len, ((uint8_t[]){ 0x5B }));
    TEST(xor(REG_EAX, REG_EDX), outbuf, outbuf_len, ((uint8_t[]){ 0x31, 0xD0 }));
    TEST(xchg(REG_ECX, REG_EBX), outbuf, outbuf_len, ((uint8_t[]){ 0x87, 0xD9 }));
    TEST(xchg(REG_EDX, REG_EAX), outbuf, outbuf_len, ((uint8_t[]){ 0x92 }));
    TEST(movrc(REG_EDX, 1337), outbuf, outbuf_len, ((uint8_t[]){ 0xBA, 0x39, 0x05, 0x00, 0x00 }));
    TEST(movrc(REG_EAX, 0xCAFEBABE), outbuf, outbuf_len, ((uint8_t[]){ 0xB8, 0xBE, 0xBA, 0xFE, 0xCA }));
    TEST(movrc_raw(REG_EBX, 0), outbuf, outbuf_len, ((uint8_t[]){ 0xBB, 0x00, 0x00, 0x00, 0x00 }));
    TEST(movrc(REG_EBX, 0), outbuf, outbuf_len, ((uint8_t[]){ 0x31, 0xDB }));
    TEST(pushc(0xCAFEBABE), outbuf, outbuf_len, ((uint8_t[]){ 0x68, 0xBE, 0xBA, 0xFE, 0xCA }));
    TEST(pushc(0x7F), outbuf, outbuf_len, ((uint8_t[]){ 0x6A, 0x7F }));
    TEST(test(REG_EAX), outbuf, outbuf_len, ((uint8_t[]){ 0x85, 0xC0 }));
    TEST(neg(REG_EBX), outbuf, outbuf_len, ((uint8_t[]){ 0xF7, 0xDB }));
    TEST(not(REG_ESI), outbuf, outbuf_len, ((uint8_t[]){ 0xF7, 0xD6 }));
    TEST(add(REG_EBX, REG_EDI), outbuf, outbuf_len, ((uint8_t[]){ 0x01, 0xFB }));
    TEST(andrc(REG_EBX, 99), outbuf, outbuf_len, ((uint8_t[]){ 0x83, 0xE3, 0x63 }));
    TEST(orrc(REG_EDI, 0), outbuf, outbuf_len, ((uint8_t[]){ 0x83, 0xCF, 0x00 }));
    TEST(xorrc(REG_EDX, 1337), outbuf, outbuf_len, ((uint8_t[]){ 0x81, 0xF2, 0x39, 0x05, 0x00, 0x00 }));
    TEST(addrc(REG_EDI, 0xBEEFC01A), outbuf, outbuf_len, ((uint8_t[]){ 0x81, 0xC7, 0x1A, 0xC0, 0xEF, 0xBE }));
    TEST(subrc(REG_EBX, 1337), outbuf, outbuf_len, ((uint8_t[]){ 0x81, 0xEB, 0x39, 0x05, 0x00, 0x00 }));
    TEST(movmr(REG_EBX, 4, REG_ECX), outbuf, outbuf_len, ((uint8_t[]){ 0x89, 0x4B, 0x04 }));
    TEST(movmr(REG_EAX, 3, REG_ESP), outbuf, outbuf_len, ((uint8_t[]){ 0x89, 0x60, 0x03 }));
    TEST(movrm(REG_EDX, 7, REG_EBP), outbuf, outbuf_len, ((uint8_t[]){ 0x8B, 0x55, 0x07 }));
    TEST(movrm(REG_EAX, 32, REG_ESP), outbuf, outbuf_len, ((uint8_t[]){ 0x8B, 0x44, 0x24, 0x20 }));
    
    // TEST(   , outbuf, outbuf_len, ((uint8_t[]){   }));
    

    printf("All tests passed SUCCESSFULLY\n");
}

int main() {
    run_tests();

}


// gcc main.c -o main.exe -Wall -Wextra && main.exe



