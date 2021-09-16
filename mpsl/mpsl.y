%{
/*

    MPSL - Minimum Profit Scripting Language
    YACC parser

    ttcdt <dev@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <stdlib.h>
#include "mpdm.h"
#include "mpsl.h"

#define COMPILER_VERSION "1.01"

/** data **/

/* the bytecode being generated */
static mpdm_t mpsl_bytecode = NULL;

/* pointer to source code being compiled */
extern wchar_t *mpsl_next_char;

/* pointer to file being compiled */
extern FILE *mpsl_file;

/* line number */
extern int mpsl_line;

/* compilation source */
mpdm_t mpsl_source = NULL;

/* cached value MPSL.LC */
extern mpdm_t mpsl_lc;


/** code **/

int yylex(void);
void yyerror(char *s);

#define INS0(o)             mpsl_mkins(o, 0, NULL, NULL, NULL, NULL)
#define INS1(o,a1)          mpsl_mkins(o, 1, a1, NULL, NULL, NULL)
#define INS2(o,a1,a2)       mpsl_mkins(o, 2, a1, a2, NULL, NULL)
#define INS3(o,a1,a2,a3)    mpsl_mkins(o, 3, a1, a2, a3, NULL)
#define INS4(o,a1,a2,a3,a4) mpsl_mkins(o, 4, a1, a2, a3, a4)


#if 0
static void compiler_warning(char *str)
{
    fprintf(stderr, "WARNING: %s.\n", str);
}
#endif


static mpdm_t code_pos(void)
{
    char tmp[1024];

    sprintf(tmp, ":%d", mpsl_line + 1);

    return INS1(L"LITERAL", mpdm_strcat(mpsl_source, MPDM_MBS(tmp)));
}


%}

%union {
    mpdm_t v;   /* a simple value */
    mpdm_t ins; /* an 'instruction': [ opcode, args ] */
};

%token <v> NULLV VER INTEGER REAL STRING SYMBOL LITERAL
%token WHILE FOR IF SUB FOREACH LOCAL GLOBAL BREAK RETURN
%nonassoc IFI
%nonassoc ELSE

%left AND OR
%left IADD ISUB IMUL IDIV IMOD ORASSIGN
%left '!'
%left EQ NE GE LE ARROW ':' '>''<'
%left '+' '-'
%left '*' '/' MOD
%left INVCALL
%left '$'
%nonassoc UMINUS

%type <ins> stmt expr sym_list stmt_list list hash compsym

%%

program:
    function        { ; }
    ;

function:
    function stmt_list  {
                    mpsl_bytecode = $2;
                }
    | /* NULL */
    ;

stmt:
    ';'         {
                    /* null instruction */
                    $$ = INS0(L"MULTI");
                }
    | expr ';'      {
                    /* expression, as is */
                    $$ = $1;
                }

    | WHILE '(' expr ')' stmt
                {
                    /* while loop */
                    $$ = INS2(L"WHILE", $3, $5);
                }
    | FOR '(' expr ';' expr ';' expr ')' stmt
                {
                    /* for loop */
                    $$ = INS4(L"WHILE", $5, $9, $3, $7);
                }
    | FOR '(' ';' ';' ')' stmt
                {
                    /* infinite loop */
                    $$ = INS2(L"WHILE", INS1(L"LITERAL", mpdm_bool(1)), $6);
                }
    | IF '(' expr ')' stmt %prec IFI
                {
                    /* if - then construction */
                    $$ = INS2(L"IF", $3, $5);
                }
    | IF '(' expr ')' stmt ELSE stmt
                {
                    /* if - then - else construction */
                    $$ = INS3(L"IF", $3, $5, $7);
                }

    | SUB compsym '{' stmt_list '}'
                {
                    /* subroutine definition,
                       without arguments */
                    $$ = INS2(L"ASSIGN", $2,
                        INS1(L"LITERAL",
                            mpsl_x($4, NULL, 1)));
                }

    | SUB compsym '(' ')' '{' stmt_list '}'
                {
                    /* subroutine definition,
                       without arguments (second
                       syntax, including parens) */
                    $$ = INS2(L"ASSIGN", $2,
                        INS1(L"LITERAL",
                            mpsl_x($6, NULL, 1)));
                }

    | SUB compsym '(' sym_list ')' '{' stmt_list '}'
                {
                    /* subroutine definition,
                       with arguments */
                    $$ = INS2(L"ASSIGN", $2,
                        INS1(L"LITERAL",
                            mpsl_x($7, $4, 1)));
                }

    | FOREACH '(' compsym ',' expr ')' stmt
                {
                    /* foreach (e, v) construction */
                    /* a block frame is created, the iterator
                       created as local, and the foreach executed */
                    $$ = INS1(L"BLKFRAME",
                        INS2(L"MULTI",
                            INS1(L"LOCAL", $3),
                            INS3(L"FOREACH", $3, $5, $7)
                        )
                    );
                }

    | FOREACH '(' compsym ',' compsym ',' expr ')' stmt
                {
                    /* foreach (k, v, a) construction */
                    /* a block frame is created, the iterator
                       created as local, and the foreach executed */
                    $$ = INS1(L"BLKFRAME",
                        INS2(L"MULTI",
                            INS1(L"LOCAL", $3),
                            INS2(L"MULTI",
                                INS1(L"LOCAL", $5),
                                INS4(L"FOREACH", $3, $7, $9, $5)
                            )
                        )
                    );
                }

    | '{' stmt_list '}' {
                    /* block of instructions,
                       with local symbol table */
                    $$ = INS1(L"BLKFRAME", $2);
                }

    | LOCAL sym_list ';'    {
                    /* local symbol creation */
                    $$ = INS1(L"LOCAL", $2);
                }
    | LOCAL SYMBOL '=' expr ';'
                {
                    /* contraction; local symbol
                       creation and assignation */
                    $$ = INS2(L"MULTI",
                        INS1(L"LOCAL",
                            INS1(L"LITERAL", $2)),
                        INS2(L"ASSIGN",
                            INS1(L"LITERAL", $2),$4)
                        );
                }
    | GLOBAL sym_list ';'   {
                    /* global symbol creation */
                    $$ = INS1(L"GLOBAL", $2);
                }
    | GLOBAL SYMBOL '=' expr    ';'
                {
                    /* contraction; global symbol
                       creation and assignation */
                    $$ = INS2(L"MULTI",
                        INS1(L"GLOBAL",
                            INS1(L"LITERAL", $2)),
                        INS2(L"ASSIGN",
                            INS1(L"LITERAL", $2),$4)
                        );
                }
    | BREAK ';'     {
                    /* break (exit from loop) */
                    $$ = INS0(L"BREAK");
                }
    | RETURN expr ';'   {
                    /* return from subroutine */
                    $$ = INS1(L"RETURN", $2);
                }
    | RETURN ';'        {
                    /* return from subroutine (void) */
                    $$ = INS0(L"RETURN");
                }
    ;

stmt_list:
    stmt            { $$ = $1; }
    | stmt_list stmt    {
                    /* sequence of instructions */
                    $$ = INS2(L"MULTI", $1, $2);
                }
    ;

list:
    expr            {
                    $$ = INS1(L"LIST", $1);
                }
    | list ',' expr     {
                    /* build list from list of
                       instructions */
                    $$ = INS2(L"LIST", $3, $1);
                }
    ;

sym_list:
    SYMBOL          {
                    $$ = INS1(L"LIST",
                        INS1(L"LITERAL", $1));
                }
    | sym_list ',' SYMBOL   {
                    /* comma-separated list of symbols */
                    $$ = INS2(L"LIST",
                        INS1(L"LITERAL", $3), $1);
                }
    ;

hash:
    expr ARROW expr {
                $$ = INS2(L"OBJECT", $1, $3);
                }
    | SYMBOL ':' expr {
                $$ = INS2(L"OBJECT", INS1(L"LITERAL", $1), $3);
                }
    | hash ',' expr ARROW expr
                {
                    /* build hash from list of
                       instructions */
                    $$ = INS3(L"OBJECT", $3, $5, $1);
                }
    | hash ',' SYMBOL ':' expr
                {
                    /* build hash from list of
                       instructions */
                    $$ = INS3(L"OBJECT", INS1(L"LITERAL", $3), $5, $1);
                }
    ;

compsym:
    SYMBOL          {
                    $$ = INS1(L"LIST",
                        INS1(L"LITERAL", $1));
                }
    | compsym '.' INTEGER   {
                    /* a.5 compound symbol */
                    $$ = INS2(L"LIST",
                        INS1(L"LITERAL", $3), $1);
                }
    | compsym '.' SYMBOL    {
                    /* a.b compound symbol */
                    $$ = INS2(L"LIST",
                        INS1(L"LITERAL", $3), $1);
                }
    | compsym '[' expr ']'  {
                    /* a["b"] or a[5] compound symbol */
                    $$ = INS2(L"LIST", $3, $1);
                }
    ;

expr:
    INTEGER         {
                    /* literal integer */
                    $$ = INS1(L"LITERAL", $1);
                }
    | STRING        {
                    /* literal string */
                    $$ = INS1(L"LITERAL", $1);
                }
    | REAL          {
                    /* literal real number */
                    $$ = INS1(L"LITERAL", $1);
                }
    | compsym       {
                    /* compound symbol */
                    $$ = INS1(L"SYMVAL", $1);
                }
    | NULLV         {
                    /* NULL value */
                    $$ = INS1(L"LITERAL", NULL);
                }
    | VER         {
                    /* compiler version */
                    $$ = INS1(L"LITERAL", MPDM_S(L"c-mpsl " COMPILER_VERSION));
                }

    | '-' expr %prec UMINUS {
                    /* unary minus */
                    $$ = INS2(L"MUL", $2, INS1(L"LITERAL", MPDM_R(-1)));
                }

                /* math operations */
    | expr '+' expr     { $$ = INS2(L"ADD", $1, $3); }
    | expr '-' expr     { $$ = INS2(L"SUB", $1, $3); }
    | expr '*' expr     { $$ = INS2(L"MUL", $1, $3); }
    | expr '/' expr     { $$ = INS2(L"DIV", $1, $3); }
    | expr MOD expr     { $$ = INS2(L"MOD", $1, $3); }

    | expr '$' expr     { $$ = INS2(L"FMT", $1, $3); }

                /* immediate math operations */
    | compsym IADD expr { $$ = INS2(L"ASSIGN", $1,
                    INS2(L"ADD",
                        INS1(L"SYMVAL", $1),
                        $3)
                    );
                }
    | compsym ISUB expr { $$ = INS2(L"ASSIGN", $1,
                    INS2(L"SUB",
                        INS1(L"SYMVAL", $1),
                        $3)
                    );
                }
    | compsym IMUL expr { $$ = INS2(L"ASSIGN", $1,
                    INS2(L"MUL",
                        INS1(L"SYMVAL", $1),
                        $3)
                    );
                }
    | compsym IDIV expr { $$ = INS2(L"ASSIGN", $1,
                    INS2(L"DIV",
                        INS1(L"SYMVAL", $1),
                        $3)
                    );
                }
    | compsym IMOD expr { $$ = INS2(L"ASSIGN", $1,
                    INS2(L"MOD",
                        INS1(L"SYMVAL", $1),
                        $3)
                    );
                }

    | compsym ORASSIGN expr { $$ = INS2(L"ASSIGN", $1,
                    INS2(L"OR",
                        INS1(L"SYMVAL", $1),
                        $3)
                    );
                }

    | '!' expr      {
                    /* boolean not */
                    $$ = INS1(L"NOT", $2);
                }
    | expr '<' expr     {
                    /* bool less than */
                    $$ = INS2(L"LT", $1, $3);
                }
    | expr '>' expr     {
                    /* bool greater than */
                    $$ = INS2(L"GT", $1, $3);
                }
    | expr LE expr   {
                    /* bool less or equal than */
                    $$ = INS2(L"LE", $1, $3);
                }
    | expr GE expr   {
                    /* bool greater or equal than */
                    $$ = INS2(L"GE", $1, $3);
                }
    | expr EQ expr       {
                    /* bool equal */
                    $$ = INS2(L"EQ", $1, $3);
                }
    | expr NE expr   {
                    /* bool non-equal */
                    $$ = INS1(L"NOT",
                        INS2(L"EQ", $1, $3));
                }

    | expr AND expr {
                    /* boolean and */
                    $$ = INS2(L"AND", $1, $3);
                }
    | expr OR expr  {
                    /* boolean or */
                    $$ = INS2(L"OR", $1, $3);
                }
 
    | SUB '{' stmt_list '}' {
                    /* anonymous subroutine (without args) */
                    $$ = INS1(L"LITERAL", mpsl_x($3, NULL, 0));
                }

    | SUB '(' sym_list ')' '{' stmt_list '}'
                {
                    /* anonymous subroutine (with args) */
                    $$ = INS1(L"LITERAL", mpsl_x($6, $3, 0));
                }

    | '(' expr ')'      {
                    /* parenthesized expression */
                    $$ = $2;
                }

    | '[' ']'       {
                    /* empty list */
                    $$ = INS1(L"LITERAL", MPDM_A(0));
                }
    | '[' list ']'      {
                    /* non-empty list */
                    $$ = $2;
                }

    | '{' '}'       {
                    /* empty hash */
                    $$ = INS1(L"LITERAL", MPDM_O());
                }
    | '{' hash '}'      {
                    /* non-empty hash */
                    $$ = $2;
                }

    | compsym '(' ')'   {
                    /* function call (without args) */
                    $$ = INS2(L"EXECSYM", $1, code_pos());
                }
    | compsym '(' list ')'  {
                    /* function call (with args) */
                    $$ = INS3(L"EXECSYM", $1, code_pos(), $3);
                }
    | expr INVCALL compsym '(' ')' {
                    /* function call with only an inverse argument */
                    $$ = INS3(L"METHOD", $3, code_pos(), INS1(L"ILIST", $1));
                }
    | expr INVCALL compsym '(' list ')' {
                    /* function call with inverse argument and other ones */
                    $$ = INS3(L"METHOD", $3, code_pos(), INS2(L"ILIST", $1, $5));
                }
    | compsym '=' expr  {
                    /* simple assignation */
                    $$ = INS2(L"ASSIGN", $1, $3);
                }

    ;

%%

void yyerror(char *s)
{
    char tmp[1024];

    snprintf(tmp, sizeof(tmp), ":%d: %s", mpsl_line + 1, s);

    mpsl_error(mpdm_strcat(mpsl_source, MPDM_MBS(tmp)));
}


static mpdm_t mpsl_c_compiler(mpdm_t code, mpdm_t source)
/* the classical (lex+yacc) compiler entry point */
{
    mpdm_t v;
    mpdm_t x = NULL;

    mpdm_ref(code);

    /* first line */
    mpsl_line = 0;

    /* reset last bytecode */
    mpsl_bytecode = NULL;

    /* set globals */
    if (mpdm_type(code) == MPDM_TYPE_FILE) {
        mpsl_next_char = NULL;
        mpsl_file      = mpdm_get_filehandle(code);
    }
    else {
        mpsl_next_char = (wchar_t *)code->data;
        mpsl_file      = NULL;
    }

    mpdm_store(&mpsl_source, source);

    /* cache some values */
    v = mpdm_get_wcs(mpdm_root(), L"MPSL");
    mpsl_lc = mpdm_get_wcs(v, L"LC");

    /* compile! */
    if (yyparse() == 0 && mpsl_bytecode != NULL)
        x = mpsl_x(mpsl_bytecode, NULL, 1);

    /* clean back cached values */
    mpsl_lc = NULL;

    mpdm_store(&mpsl_source, NULL);

    mpdm_unref(code);

    return x;
}


mpdm_t mpsl_c_compiler_x(mpdm_t args, mpdm_t ctxt)
{
    return mpsl_c_compiler(mpdm_get_i(args, 0), mpdm_get_i(args, 1));
}
