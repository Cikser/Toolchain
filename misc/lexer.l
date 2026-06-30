%{
#include "../inc/as.hpp"
#include "parser.tab.h"
#include <cstdlib>
#include <cstring>
%}

%option noyywrap
%option yylineno

%%

[ \t\r]+                { }
#[^\n]*                 { }
\n                      { }

"halt"                  { return TOK_HALT; }
"int"                   { return TOK_INT; }
"iret"                  { return TOK_IRET; }
"ret"                   { return TOK_RET; }
"call"                  { return TOK_CALL; }
"jmp"                   { return TOK_JMP; }
"beq"                   { return TOK_BEQ; }
"bne"                   { return TOK_BNE; }
"bgt"                   { return TOK_BGT; }
"push"                  { return TOK_PUSH; }
"pop"                   { return TOK_POP; }
"xchg"                  { return TOK_XCHG; }
"add"                   { return TOK_ADD; }
"sub"                   { return TOK_SUB; }
"mul"                   { return TOK_MUL; }
"div"                   { return TOK_DIV; }
"not"                   { return TOK_NOT; }
"and"                   { return TOK_AND; }
"or"                    { return TOK_OR; }
"xor"                   { return TOK_XOR; }
"shl"                   { return TOK_SHL; }
"shr"                   { return TOK_SHR; }
"ld"                    { return TOK_LD; }
"st"                    { return TOK_ST; }
"csrrd"                 { return TOK_CSRRD; }
"csrwr"                 { return TOK_CSRWR; }

".global"               { return TOK_DGLOBAL; }
".extern"               { return TOK_DEXTERN; }
".section"              { return TOK_DSECTION; }
".word"                 { return TOK_DWORD; }
".skip"                 { return TOK_DSKIP; }
".ascii"                { return TOK_DASCII; }
".equ"                  { return TOK_DEQU; }
".end"                  { return TOK_DEND; }

"%r0"                   { yylval.reg = 0;  return TOK_GPR; }
"%r1"                   { yylval.reg = 1;  return TOK_GPR; }
"%r2"                   { yylval.reg = 2;  return TOK_GPR; }
"%r3"                   { yylval.reg = 3;  return TOK_GPR; }
"%r4"                   { yylval.reg = 4;  return TOK_GPR; }
"%r5"                   { yylval.reg = 5;  return TOK_GPR; }
"%r6"                   { yylval.reg = 6;  return TOK_GPR; }
"%r7"                   { yylval.reg = 7;  return TOK_GPR; }
"%r8"                   { yylval.reg = 8;  return TOK_GPR; }
"%r9"                   { yylval.reg = 9;  return TOK_GPR; }
"%r10"                  { yylval.reg = 10; return TOK_GPR; }
"%r11"                  { yylval.reg = 11; return TOK_GPR; }
"%r12"                  { yylval.reg = 12; return TOK_GPR; }
"%r13"                  { yylval.reg = 13; return TOK_GPR; }
"%r14"                  { yylval.reg = 14; return TOK_GPR; }
"%r15"                  { yylval.reg = 15; return TOK_GPR; }
"%sp"                   { yylval.reg = 14; return TOK_GPR; }
"%pc"                   { yylval.reg = 15; return TOK_GPR; }

"%status"               { yylval.reg = 0; return TOK_CSR; }
"%handler"              { yylval.reg = 1; return TOK_CSR; }
"%cause"                { yylval.reg = 2; return TOK_CSR; }

0x[0-9a-fA-F]+          {
                            yylval.lit = (int32_t)strtol(yytext + 2, nullptr, 16);
                            return TOK_LITERAL;
                        }
-?[0-9]+                {
                            yylval.lit = (int32_t)strtol(yytext, nullptr, 10);
                            return TOK_LITERAL;
                        }

\"([^\"\\]|\\.)*\"      {
                            std::string s(yytext + 1, strlen(yytext) - 2);
                            yylval.str = strdup(s.c_str());
                            return TOK_STRING;
                        }

[a-zA-Z_][a-zA-Z0-9_]* {
                            yylval.str = strdup(yytext);
                            return TOK_SYMBOL;
                        }

","                     { return TOK_COMMA; }
":"                     { return TOK_COLON; }
"$"                     { return TOK_DOLLAR; }
"["                     { return TOK_LBRACKET; }
"]"                     { return TOK_RBRACKET; }
"+"                     { return TOK_PLUS; }
"-"                     { return TOK_MINUS; }
"*"                     { return TOK_STAR; }
"/"                     { return TOK_SLASH; }
"("                     { return TOK_LPAREN; }
")"                     { return TOK_RPAREN; }

.                       { 
                            fprintf(stderr, "Unknown token: %s\n", yytext);
                            return TOK_UNKNOWN;
                        }

%%