%{
#include "../inc/as.hpp"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern int yylex();
extern int yylineno;
void yyerror(as::assembler* as, const char* msg) {
    fprintf(stderr, "Parse error at line %d: %s\n", yylineno, msg);
}
%}

%parse-param { as::assembler* as }

%union {
    int32_t     reg;
    int32_t     lit;
    char*       str;
    as::operand_t*                    operand;
    std::vector<std::string>*         str_list;
    std::vector<as::value_t>*         word_list;
    as::expr_node_t*                  expr;
}

%token TOK_HALT TOK_INT TOK_IRET TOK_RET
%token TOK_CALL TOK_JMP
%token TOK_BEQ TOK_BNE TOK_BGT
%token TOK_PUSH TOK_POP
%token TOK_XCHG
%token TOK_ADD TOK_SUB TOK_MUL TOK_DIV
%token TOK_NOT
%token TOK_AND TOK_OR TOK_XOR
%token TOK_SHL TOK_SHR
%token TOK_LD TOK_ST
%token TOK_CSRRD TOK_CSRWR

%token TOK_DGLOBAL TOK_DEXTERN TOK_DSECTION
%token TOK_DWORD TOK_DSKIP TOK_DASCII
%token TOK_DEQU TOK_DEND

%token <reg> TOK_GPR
%token <reg> TOK_CSR
%token <lit> TOK_LITERAL
%token <str> TOK_SYMBOL
%token <str> TOK_STRING

%token TOK_COMMA TOK_COLON TOK_DOLLAR
%token TOK_LBRACKET TOK_RBRACKET TOK_PLUS TOK_MINUS TOK_STAR TOK_SLASH
%token TOK_UNKNOWN

%type <operand>   operand
%type <str_list>  symbol_list
%type <word_list> word_list
%type <expr>      equ_expr

%left TOK_PLUS TOK_MINUS
%left TOK_STAR TOK_SLASH
%right UMINUS

%%


program
    : 
    | program line
    ;

line
    : label_def
    | label_def statement
    | statement
    ;

label_def
    : TOK_SYMBOL TOK_COLON
        { as->define_label($1); free($1); }
    ;

statement
    : directive
    | instruction
    ;

equ_expr
    : equ_expr TOK_PLUS equ_expr
        {
            auto* n = new as::expr_node_t();
            n->is_binary = true;
            n->type = as::expr_type::ADD;
            n->left = std::shared_ptr<as::expr_node_t>($1);
            n->right = std::shared_ptr<as::expr_node_t>($3);
            $$ = n;
        }
    | equ_expr TOK_MINUS equ_expr
        {
            auto* n = new as::expr_node_t();
            n->is_binary = true;
            n->type = as::expr_type::SUB;
            n->left = std::shared_ptr<as::expr_node_t>($1);
            n->right = std::shared_ptr<as::expr_node_t>($3);
            $$ = n;
        }
    | equ_expr TOK_STAR equ_expr
        {
            auto* n = new as::expr_node_t();
            n->is_binary = true;
            n->type = as::expr_type::MUL;
            n->left = std::shared_ptr<as::expr_node_t>($1);
            n->right = std::shared_ptr<as::expr_node_t>($3);
            $$ = n;
        }
    | equ_expr TOK_SLASH equ_expr
        {
            auto* n = new as::expr_node_t();
            n->is_binary = true;
            n->type = as::expr_type::DIV;
            n->left = std::shared_ptr<as::expr_node_t>($1);
            n->right = std::shared_ptr<as::expr_node_t>($3);
            $$ = n;
        }
    | TOK_MINUS equ_expr %prec UMINUS
        {
            auto* zero = new as::expr_node_t();
            zero->is_literal = true;
            zero->literal = 0;
            auto* n = new as::expr_node_t();
            n->is_binary = true;
            n->type = as::expr_type::SUB;
            n->left = std::shared_ptr<as::expr_node_t>(zero);
            n->right = std::shared_ptr<as::expr_node_t>($2);
            $$ = n;
        }
    | TOK_LITERAL
        {
            auto* n = new as::expr_node_t();
            n->is_literal = true;
            n->literal = $1;
            $$ = n;
        }
    | TOK_SYMBOL
        {
            auto* n = new as::expr_node_t();
            n->is_literal = false;
            n->is_binary = false;
            n->symbol = $1;
            free($1);
            $$ = n;
        }
    ;

directive
    : TOK_DGLOBAL symbol_list
        { as->dir_global(*$2); delete $2; }
    | TOK_DEXTERN symbol_list
        { as->dir_extern(*$2); delete $2; }
    | TOK_DSECTION TOK_SYMBOL
        { as->dir_section($2); free($2); }
    | TOK_DWORD word_list
        { as->dir_word(*$2); delete $2; }
    | TOK_DSKIP TOK_LITERAL
        { as->dir_skip((uint32_t)$2); }
    | TOK_DASCII TOK_STRING
        { as->dir_ascii($2); free($2); }
    | TOK_DEQU TOK_SYMBOL TOK_COMMA equ_expr
        { as->dir_equ($2, std::shared_ptr<as::expr_node_t>($4)); }
    | TOK_DEND
        { as->dir_end(); YYACCEPT; }
    ;

symbol_list
    : TOK_SYMBOL
        {
            $$ = new std::vector<std::string>();
            $$->push_back($1);
            free($1);
        }
    | symbol_list TOK_COMMA TOK_SYMBOL
        {
            $1->push_back($3);
            free($3);
            $$ = $1;
        }
    ;

word_list
    : TOK_LITERAL
        {
            $$ = new std::vector<as::value_t>();
            $$->push_back((int32_t)$1);
        }
    | TOK_SYMBOL
        {
            $$ = new std::vector<as::value_t>();
            $$->push_back(std::string($1));
            free($1);
        }
    | word_list TOK_COMMA TOK_LITERAL
        {
            $1->push_back((int32_t)$3);
            $$ = $1;
        }
    | word_list TOK_COMMA TOK_SYMBOL
        {
            $1->push_back(std::string($3));
            free($3);
            $$ = $1;
        }
    ;

operand
    : TOK_DOLLAR TOK_LITERAL
        {
            $$ = new as::operand_t();
            $$->type    = as::operand_type::LITERAL_IMM;
            $$->literal = $2;
            $$->reg     = -1;
        }
    | TOK_DOLLAR TOK_SYMBOL
        {
            $$ = new as::operand_t();
            $$->type   = as::operand_type::SYMBOL_IMM;
            $$->symbol = $2;
            $$->reg    = -1;
            free($2);
        }
    | TOK_LITERAL
        {
            $$ = new as::operand_t();
            $$->type    = as::operand_type::LITERAL_MEM;
            $$->literal = $1;
            $$->reg     = -1;
        }
    | TOK_SYMBOL
        {
            $$ = new as::operand_t();
            $$->type   = as::operand_type::SYMBOL_MEM;
            $$->symbol = $1;
            $$->reg    = -1;
            free($1);
        }
    | TOK_GPR
        {
            $$ = new as::operand_t();
            $$->type = as::operand_type::REG_DIRECT;
            $$->reg  = $1;
        }
    | TOK_LBRACKET TOK_GPR TOK_RBRACKET
        {
            $$ = new as::operand_t();
            $$->type = as::operand_type::REG_INDIRECT;
            $$->reg  = $2;
        }
    | TOK_LBRACKET TOK_GPR TOK_PLUS TOK_LITERAL TOK_RBRACKET
        {
            $$ = new as::operand_t();
            $$->type    = as::operand_type::REG_OFFSET_LIT;
            $$->reg     = $2;
            $$->literal = $4;
        }
    | TOK_LBRACKET TOK_GPR TOK_PLUS TOK_SYMBOL TOK_RBRACKET
        {
            $$ = new as::operand_t();
            $$->type   = as::operand_type::REG_OFFSET_SYM;
            $$->reg    = $2;
            $$->symbol = $4;
            free($4);
        }
    ;

instruction
    : TOK_HALT
        { as->instr_halt(); }
    | TOK_INT
        { as->instr_int(); }
    | TOK_IRET
        { as->instr_iret(); }
    | TOK_RET
        { as->instr_ret(); }
    | TOK_CALL operand
        { as->instr_call(*$2); delete $2; }
    | TOK_JMP operand
        { as->instr_jmp(*$2); delete $2; }
    | TOK_BEQ TOK_GPR TOK_COMMA TOK_GPR TOK_COMMA operand
        { as->instr_beq($2, $4, *$6); delete $6; }
    | TOK_BNE TOK_GPR TOK_COMMA TOK_GPR TOK_COMMA operand
        { as->instr_bne($2, $4, *$6); delete $6; }
    | TOK_BGT TOK_GPR TOK_COMMA TOK_GPR TOK_COMMA operand
        { as->instr_bgt($2, $4, *$6); delete $6; }
    | TOK_PUSH TOK_GPR
        { as->instr_push($2); }
    | TOK_POP TOK_GPR
        { as->instr_pop($2); }
    | TOK_XCHG TOK_GPR TOK_COMMA TOK_GPR
        { as->instr_xchg($2, $4); }
    | TOK_ADD TOK_GPR TOK_COMMA TOK_GPR
        { as->instr_add($2, $4); }
    | TOK_SUB TOK_GPR TOK_COMMA TOK_GPR
        { as->instr_sub($2, $4); }
    | TOK_MUL TOK_GPR TOK_COMMA TOK_GPR
        { as->instr_mul($2, $4); }
    | TOK_DIV TOK_GPR TOK_COMMA TOK_GPR
        { as->instr_div($2, $4); }
    | TOK_NOT TOK_GPR
        { as->instr_not($2); }
    | TOK_AND TOK_GPR TOK_COMMA TOK_GPR
        { as->instr_and($2, $4); }
    | TOK_OR TOK_GPR TOK_COMMA TOK_GPR
        { as->instr_or($2, $4); }
    | TOK_XOR TOK_GPR TOK_COMMA TOK_GPR
        { as->instr_xor($2, $4); }
    | TOK_SHL TOK_GPR TOK_COMMA TOK_GPR
        { as->instr_shl($2, $4); }
    | TOK_SHR TOK_GPR TOK_COMMA TOK_GPR
        { as->instr_shr($2, $4); }
    | TOK_LD operand TOK_COMMA TOK_GPR
        { as->instr_ld(*$2, $4); delete $2; }
    | TOK_ST TOK_GPR TOK_COMMA operand
        { as->instr_st($2, *$4); delete $4; }
    | TOK_CSRRD TOK_CSR TOK_COMMA TOK_GPR
        { as->instr_csrrd($2, $4); }
    | TOK_CSRWR TOK_GPR TOK_COMMA TOK_CSR
        { as->instr_csrwr($2, $4); }
    ;

%%