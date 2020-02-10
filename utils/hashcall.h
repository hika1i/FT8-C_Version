//
// Created by PYL on 2020/2/10.
//

#ifndef FT8_C_VERSION_HASHCALL_H
#define FT8_C_VERSION_HASHCALL_H

#include <stdint.h>
extern char mycall13[13];
extern const char A0[];

//--------------------------------
//FOR HASH22
#define CALL22_MAX 40
extern uint32_t  CALL22_Hash[CALL22_MAX];
extern char CALL22_Text[CALL22_MAX][13];

//--------------------------------
//FOR HASH12
#define CALL12_MAX 40
extern uint32_t  CALL12_Hash[CALL12_MAX];
extern char CALL12_Text[CALL12_MAX][13];

//--------------------------------
//FOR HASH10
#define CALL10_MAX 40
extern uint32_t  CALL10_Hash[CALL10_MAX];
extern char CALL10_Text[CALL10_MAX][13];

int StrTrim(char *line);
uint32_t ihashcall(char *c0, int m);
void save_hash_call(char *c13);
void hash12(uint32_t n12, char * c13);
void hash22(uint32_t n22, char * c13);

#endif //FT8_C_VERSION_HASHCALL_H
