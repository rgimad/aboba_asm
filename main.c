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

void mov(int reg1, int reg2) {
    oprr(0x89, reg1, reg2);
}

void xchg(int reg1, int reg2) {
    if (reg1 == REG_EAX || reg2 == REG_EAX) {
        out_byte(0x90 + reg1 + reg2);
    } else {
        oprr(0x87, reg1, reg2);
    }
}

void pop(int reg) {
    out_byte(0x58 + reg);
}

void push(int reg) {
    out_byte(0x50 + reg);
}

void xor(int reg1, int reg2) {
    oprr(0x31, reg1, reg2);
}

void movrc_raw(int reg, int n) {
    out_byte(0xB8 + reg);
    out_int(n);
}

void movrc(int reg, int n) {
    if (n == 0) {
        xor(reg, reg);
    } else {
        movrc_raw(reg, n);
    }
}

void pushc(int n) {
    out_byte(0x68 + (is_byte(n) ? 2 : 0));
    out_int_or_byte(n);
}

// test reg, reg
void test(int reg) {
    out_byte2(0x85, 0xC0 + reg*9);
}

void neg(int reg) {
    out_byte2(0xF7, 0xD8 + reg);
}

void not(int reg) {
    out_byte2(0xF7, 0xD0 + reg);
}

void add(int reg1, int reg2) {
    oprr(0x1, reg1, reg2);
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


    // TEST(   , outbuf, outbuf_len, ((uint8_t[]){   }));
    

    printf("All tests passed SUCCESSFULLY\n");
}

int main() {
    run_tests();

}


// gcc main.c -o main.exe -Wall -Wextra && main.exe



