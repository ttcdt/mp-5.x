/*

    MPSL - Minimum Profit Scripting Language
    Ad-hoc compiler: lexer + parser

    ttcdt <dev@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include "config.h"

#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <stdlib.h>

#include "mpdm.h"
#include "mpsl.h"

#define COMPILER_VERSION "1.00"

/** compiler structure **/

typedef enum {
    T_EOP,      T_ERROR,
    T_OCURLY,   T_CCURLY,  T_OPAREN,  T_CPAREN,  T_OBRACK,  T_CBRACK,
    T_COLON,    T_SEMI,    T_DOT,     T_COMMA,
    T_EQ,       T_BANG,
    T_GT,       T_LT,      T_DPIPE,   T_DAMP,    T_DDOT,
    T_DEQ,      T_BANGEQ,  T_GTEQ,    T_LTEQ, 
    T_PLUS,     T_MINUS,   T_ASTER,   T_SLASH,   T_PERCENT,  
    T_PIPE,     T_AMP,     T_CARET,   T_DOLLAR,
    T_SQUOTE,   T_DQUOTE,
    T_PIPEEQ,   T_AMPEQ,   T_CARETEQ,
    T_DGT,      T_DLT,     T_DPLUS,   T_DMINUS,
    T_PLUSEQ,   T_MINUSEQ, T_ASTEREQ, T_SLASHEQ, T_PERCEQ,
    T_DGTEQ,    T_DLTEQ,   T_DPIPEEQ, T_DAMPEQ,
    T_THINARRW, T_FATARRW,
    T_VIRGULE,  T_VIREQ,
    T_NULL,     T_VER,
    T_WHILE,    T_FOR,      T_FOREACH,
    T_BREAK,    T_RETURN,
    T_IF,       T_ELSE,
    T_SUB,      T_LOCAL,    T_GLOBAL,
    T_BAREWORD, T_STRING,   T_INTEGER,  T_REAL
} mpsl_token_t;

struct mpsl_a {
    mpsl_token_t token; /* token found */
    wchar_t *token_s;   /* token as string */
    int token_z;        /* token size */
    char *mbs;          /* helping multibyte string */
    int mbs_z;          /* size of multibyte string */
    mpdm_t val;         /* the token as a value */
    int x;              /* x source position */
    int y;              /* y source position */
    wchar_t c;          /* last char read from input */
    wchar_t *ptr;       /* program source */
    FILE *f;            /* program file */
    int error;          /* non-zero if syntax error */
    mpdm_t source;      /* code source */
};


/** lexer **/

static wchar_t nc(struct mpsl_a *c)
/* gets the next char */
{
    if (c->f) {
        int s = 0;
        int cc;

        while ((cc = getc(c->f)) != EOF && mpdm_utf8_to_wc(&c->c, &s, cc));

        if (cc == EOF || s)
            c->c = L'\0';
    }
    else
        c->c = *(c->ptr++);

    /* update position in source */
    if (c->c == L'\n') {
        c->y++;
        c->x = 0;
    }
    else
        c->x++;

    return c->c;
}


static void c_error(struct mpsl_a *c)
/* compiler error */
{
    char tmp[1024];

    sprintf(tmp, ":%d:%d: syntax error", c->y + 1, c->x + 1);
    mpdm_set_wcs(mpdm_root(), mpdm_strcat(c->source, MPDM_MBS(tmp)), L"ERROR");

    c->error = 1;
}


static void POKE(struct mpsl_a *c, wchar_t k)
/* poke one char into current token */
{
    c->token_s = mpdm_pokewsn(c->token_s, &c->token_z, &k, 1);
}


static void POKE_MBS(struct mpsl_a *c, wchar_t k)
/* poke one char into the work mbs */
{
    c->mbs = realloc(c->mbs, c->mbs_z + 2);
    c->mbs[c->mbs_z++] = (char) (k & 0xff);
    c->mbs[c->mbs_z] = '\0';
}


#define POKE_WHILE(COND) while (COND) { POKE(c, c->c); nc(c); }

#define POKE_MBS_WHILE(COND) while (COND) { POKE_MBS(c, c->c); nc(c); }

#define BLANK(c) ((c) == L' ' || (c) == L'\t' || (c) == L'\r' || (c) == L'\n')
#define DIGIT(d) ((d) >= L'0' && (d) <= L'9')
#define ALPHA(a) ((a) == L'_' || (((a) >= L'a') && ((a) <= L'z')) || (((a) >= L'A') && ((a) <= L'Z')))
#define ALNUM(d) (DIGIT(d) || ALPHA(d))
#define HEXDG(h) (DIGIT(h) || ((h) >= L'a' && (h) <= L'f') || ((h) >= L'A' && (h) <= L'F'))
#define OCTDG(h) ((h) >= L'0' && (h) <= L'7')

#define IS_STR_TOKEN(s, v) if (t == T_ERROR && wcscmp(c->token_s, s) == 0) t = v


static mpsl_token_t c_to_token(struct mpsl_a *c)
/* converts a char or seq of chars to a token */
{
    int n;
    mpsl_token_t t = T_ERROR;

    struct {
        wchar_t *cs;        /* possible continuations */
        mpsl_token_t ts[8]; /* tokens */
    } c2t[] = {
        { L"{",    { T_OCURLY } },
        { L"}",    { T_CCURLY } },
        { L"(",    { T_OPAREN } },
        { L")",    { T_CPAREN } },
        { L"[",    { T_OBRACK } },
        { L"]",    { T_CBRACK } },
        { L":",    { T_COLON } },
        { L";",    { T_SEMI } },
        { L",",    { T_COMMA } },
        { L"$",    { T_DOLLAR } },
        { L"!=",   { T_BANGEQ, T_BANG } },
        { L"..",   { T_DDOT, T_DOT } },
        { L"*=",   { T_ASTEREQ, T_ASTER } },
        { L"%=",   { T_PERCEQ, T_PERCENT } },
        { L"&&",   { T_DAMP, T_AMP } },
        { L"||=",  { T_DPIPE, T_PIPEEQ, T_PIPE } },
        { L"==>",  { T_DEQ, T_FATARRW, T_EQ } },
        { L"++=",  { T_DPLUS, T_PLUSEQ, T_PLUS } },
        { L">=>",  { T_GTEQ, T_DGT, T_GT } },
        { L"<=<",  { T_LTEQ, T_DLT, T_LT } },
        { L"--=>", { T_DMINUS, T_MINUSEQ, T_THINARRW, T_MINUS } },

        { NULL,    { T_ERROR } }
    };

    for (n = 0; t == T_ERROR && c2t[n].cs; n++) {
        if (c2t[n].cs[0] == c->c) {
            /* this is the set: now pick the matching, or the last one */
            int m;
            wchar_t w;

            nc(c);

            for (m = 1; (w = c2t[n].cs[m]) && w != c->c; m++);

            /* if something was found, pick nc */
            if (w)
                nc(c);

            t = c2t[n].ts[m - 1];
        }
    }

    return t;
}


static mpsl_token_t str_to_token(struct mpsl_a *c)
/* converts the stored alpha string to a token */
{
    int n;
    struct {
        wchar_t *s;         /* the string */
        mpsl_token_t t;     /* the token */
    } s2t[] = {
        { L"NULL",      T_NULL },
        { L"while",     T_WHILE },
        { L"for",       T_FOR },
        { L"foreach",   T_FOREACH },
        { L"break",     T_BREAK },
        { L"return",    T_RETURN },
        { L"if",        T_IF },
        { L"else",      T_ELSE },
        { L"sub",       T_SUB },
        { L"local",     T_LOCAL },
        { L"global",    T_GLOBAL },
        { L"__VER__",   T_VER },
        { NULL,         T_BAREWORD }
    };

    for (n = 0; s2t[n].s && wcscmp(c->token_s, s2t[n].s); n++);

    return s2t[n].t;
}


static mpsl_token_t next_token(struct mpsl_a *c)
{
    mpsl_token_t t;

    free(c->token_s);
    c->val     = NULL;
    c->token_s = NULL;
    c->token_z = 0;

again:
    if (c->c == L'\0') {
        t = T_EOP;
        goto end;
    }

    /* blanks */
    if (BLANK(c->c)) {
        nc(c);
        goto again;
    }

    /* pick a direct char(s) to token */
    t = c_to_token(c);

    /* nothing yet? try special cases */
    if (t != T_ERROR) {}
    else
    if (c->c == L'/') {
        /* slash; may be an operator or some kind of comment */
        nc(c);

        if (c->c == L'*') {
            /* C-like comment */
            nc(c);

            while (c->c != L'\0') {
                if (c->c == L'*') {
                    if (nc(c) == L'/')
                        break;
                }
                else
                    nc(c);
            }

            nc(c);
            goto again;
        }
        else
        if (c->c == L'/') {
            /* C++-like comment */
            nc(c);

            while (c->c != L'\n' && c->c != L'\0')
                nc(c);

            goto again;
        }
        else
        if (c->c == L'=') {
            t = T_SLASHEQ;
            nc(c);
        }
        else
            t = T_SLASH;
    }
    else
    if (c->c == L'\'') {
        /* literal string */
        nc(c);
        POKE_WHILE(c->c != L'\'' && c->c != L'\0');
        nc(c);
        t = T_STRING;
    }
    else
    if (c->c == L'"') {
        /* double-quoted, escaped string */

dq_string:
        nc(c);
        while (c->c != L'"' && c->c != L'\0') {
            wchar_t m = c->c;

            if (m == L'\\') {
                m = nc(c);
                switch (m) {
                case L'n': m = L'\n';   break;
                case L'r': m = L'\r';   break;
                case L't': m = L'\t';   break;
                case L'e': m = 27;      break;
                case L'\\': m = L'\\';  break;
                case L'"': m = L'"';    break;
                case L'x':
                    if (nc(c) == L'{') {
                        int i;
                        char hex[9];

                        for (i = 0; i < sizeof(hex) && nc(c) != L'}' && c->c; i++)
                            hex[i] = (char) c->c;

                        hex[i] = '\0';

                        if (i == sizeof(hex))
                            c_error(c);
                        else
                            sscanf(hex, "%x", (int *)&m);
                    }
                    else
                        c_error(c);

                    break;
                default: POKE(c, L'\\');  break;
                }
            }
            POKE(c, m);
            nc(c);
        }

        nc(c);

        /* special case: an escaped newline; the string
           re-syncs after blanks and a new double quote */
        if (c->c == '\\') {
            if (nc(c) == '\n') {
                /* skip blanks */
                while (BLANK(c->c))
                    nc(c);

                /* string continuation */
                if (c->c == L'"')
                    goto dq_string;
                else
                    c_error(c);
            }
            else
                c_error(c);
        }

        t = T_STRING;
    }
    else
    if (DIGIT(c->c)) {
        /* numeral literal */
        t = T_INTEGER;

        c->mbs   = realloc(c->mbs, 0);
        c->mbs_z = 0;

        if (c->c == L'0') {
            POKE_MBS(c, c->c);
            nc(c);

            if (c->c == L'b' || c->c == L'B') {
                POKE_MBS(c, c->c);
                nc(c);

                POKE_MBS_WHILE(c->c == L'0' || c->c == L'1');
            }
            else
            if (c->c == L'x' || c->c == L'X') {
                POKE_MBS(c, c->c);
                nc(c);

                POKE_MBS_WHILE(HEXDG(c->c));
            }
            else
            if (OCTDG(c->c)) {
                POKE_MBS_WHILE(OCTDG(c->c));
            }
        }
        else {
            POKE_MBS_WHILE(DIGIT(c->c));
        }

        if (c->c == L'.') {
            t = T_REAL;

            POKE_MBS(c, c->c);
            nc(c);

            POKE_MBS_WHILE(DIGIT(c->c));
        }
        else
        if (c->c == L'e' || c->c == L'E') {
            t = T_REAL;

            POKE_MBS(c, c->c);
            nc(c);

            /* 1e5 or 1e-5 */
            if (c->c == L'-') {
                POKE_MBS(c, c->c);
                nc(c);
            }

            POKE_MBS_WHILE(DIGIT(c->c));
        }
    }
    else
    if (ALPHA(c->c)) {
        /* bareword (alpha token or symbol) */
        POKE_WHILE(ALNUM(c->c));

        /* look for special alpha tokens, otherwise is a bareword */
        t = str_to_token(c);
    }
    else
        t = T_ERROR;

end:
    if (t == T_ERROR)
        c_error(c);

    /* create a value, if applicable */
    switch (t) {
    case T_STRING:
    case T_BAREWORD:
        c->val = MPDM_S(c->token_s ? c->token_s : L"");
        break;

    case T_INTEGER:
        c->val = MPDM_I(mpdm_ival_mbs(c->mbs));
        break;

    case T_REAL:
        c->val = MPDM_R(mpdm_rval_mbs(c->mbs));
        break;

    default:
        break;
    }

    free(c->mbs);
    c->mbs = NULL;

    return c->token = t;
}


/** parser **/

typedef enum {
    /* order matters (operator precedence) */
    O_UMINUS,
    O_METHOD, O_NOT, O_FMT,
    O_MOD,    O_DIV,    O_MUL,  O_SUB,  O_ADD,
    O_EQ,     O_NE,     O_GT,   O_GE,   O_LT,   O_LE,
    O_AND,    O_OR,
    O_ASSIGN,
    O_SYMVAL,
    O_ANONSUB,
    O_EOP,
    O_LOWEST
} mpsl_op_t;


static mpsl_op_t op_by_token(struct mpsl_a *c)
/* returns the operator by token */
{
    int n;
    static mpsl_token_t tokens[] = {
        T_PLUS,     T_MINUS,    T_ASTER,    T_SLASH,    T_PERCENT,
        T_DEQ,      T_BANGEQ,   T_GT,       T_GTEQ,     T_LT,       T_LTEQ,
        T_DAMP,     T_DPIPE,    T_THINARRW, T_SUB,      T_DOLLAR,
        T_ERROR
    };
    static mpsl_op_t ops[] = {
        O_ADD,      O_SUB,      O_MUL,      O_DIV,      O_MOD,
        O_EQ,       O_NE,       O_GT,       O_GE,       O_LT,       O_LE,
        O_AND,      O_OR,       O_METHOD,   O_ANONSUB,  O_FMT,
        O_LOWEST
    };

    for (n = 0; tokens[n] != c->token && tokens[n] != T_ERROR; n++);

    return ops[n];
}


#define INS0(o)             mpsl_mkins(o, 0, NULL, NULL, NULL, NULL)
#define INS1(o,a1)          mpsl_mkins(o, 1, a1, NULL, NULL, NULL)
#define INS2(o,a1,a2)       mpsl_mkins(o, 2, a1, a2, NULL, NULL)
#define INS3(o,a1,a2,a3)    mpsl_mkins(o, 3, a1, a2, a3, NULL)
#define INS4(o,a1,a2,a3,a4) mpsl_mkins(o, 4, a1, a2, a3, a4)

static mpdm_t code_pos(struct mpsl_a *c)
{
    char tmp[1024];

    sprintf(tmp, ":%d", c->y + 1);

    return INS1(L"LITERAL", mpdm_strcat(c->source, MPDM_MBS(tmp)));
}


static mpdm_t object(struct mpsl_a *c, mpdm_t h);
static mpdm_t list(struct mpsl_a *c);
static mpdm_t expr(struct mpsl_a *c);
static mpdm_t expr_p(struct mpsl_a *c, mpsl_op_t op);
static mpdm_t statement(struct mpsl_a *c);
static mpdm_t statement_list(struct mpsl_a *c, int sf);
static mpdm_t symbol(struct mpsl_a *c);


static mpdm_t bareword_list(struct mpsl_a *c)
{
    mpdm_t v = NULL;

    if (c->token == T_BAREWORD) {
        v = INS1(L"LIST", INS1(L"LITERAL", c->val));
        next_token(c);

        while (!c->error && c->token == T_COMMA) {
            next_token(c);

            if (c->token == T_BAREWORD) {
                v = INS2(L"LIST", INS1(L"LITERAL", c->val), v);
                next_token(c);
            }
            else
                c_error(c);
        }
    }
    else
        c_error(c);

    return v;
}

static mpdm_t term(struct mpsl_a *c)
/* returns a term (part of an expression) */
{
    mpdm_t v = NULL;

    if (c->error) {}
    else
    if (c->token == T_BANG) {
        next_token(c);

        v = INS1(L"NOT", expr_p(c, O_NOT));
    }
    else
    if (c->token == T_MINUS) {
        next_token(c);

        v = INS2(L"MUL", expr_p(c, O_UMINUS), INS1(L"LITERAL", MPDM_R(-1)));
    }
    else
    if (c->token == T_NULL) {
        next_token(c);

        v = INS1(L"LITERAL", NULL);
    }
    else
    if (c->token == T_VER) {
        next_token(c);

        v = INS1(L"LITERAL", MPDM_S(L"a-mpsl " COMPILER_VERSION));
    }
    else
    if (c->token == T_OPAREN) {
        /* parentesized expression */
        next_token(c);
        v = expr(c);

        if (c->token == T_CPAREN)
            next_token(c);
        else
            c_error(c);
    }
    else
    if (c->token == T_OCURLY) {
        /* inline object */
        next_token(c);

        if (c->token == T_CCURLY) {
            /* empty object */
            v = INS0(L"OBJECT");
            next_token(c);
        }
        else {
            v = object(c, NULL);
            next_token(c);
        }
    }
    else
    if (c->token == T_OBRACK) {
        /* inline array */
        next_token(c);

        if (c->token == T_CBRACK) {
            /* empty array */
            v = INS0(L"LIST");
            next_token(c);
        }
        else {
            v = list(c);

            if (c->token == T_CBRACK)
                next_token(c);
            else
                c_error(c);
        }
    }
    else
    if (c->token == T_SUB) {
        /* anonymous code */
        next_token(c);

        if (c->token == T_OPAREN) {
            /* anonymous code with parens */
            next_token(c);

            if (c->token == T_CPAREN) {
                /* anonymous code with empty argument list */
                next_token(c);

                if (c->token == T_OCURLY) {
                    v = statement_list(c, 0);
    
                    if (v && !c->error)
                        v = INS1(L"LITERAL", MPDM_X2(mpsl_exec_p, v));
                }
                else
                    c_error(c);
            }
            else {
                /* anonymous code with argument list */
                mpdm_t a = bareword_list(c);

                if (a && !c->error && c->token == T_CPAREN) {
                    next_token(c);

                    if (c->token == T_OCURLY) {
                        v = statement_list(c, 0);
        
                        if (v && !c->error) {
                            mpdm_push(v, a);
                            v = INS1(L"LITERAL", MPDM_X2(mpsl_exec_p, v));
                        }
                    }
                    else
                        c_error(c);
                }
            }
        }
        else {
            /* anonymous code with no arguments */
            if (c->token == T_OCURLY) {
                v = statement_list(c, 0);

                if (v && !c->error)
                    v = INS1(L"LITERAL", MPDM_X2(mpsl_exec_p, v));
            }
            else
                c_error(c);
        }
    }
    else
    if (c->token == T_STRING || c->token == T_INTEGER || c->token == T_REAL) {
        /* literal value */
        v = c->val;
        next_token(c);

        v = INS1(L"LITERAL", v);
    }

    return v;
}


static mpdm_t symbol(struct mpsl_a *c)
/* parse a symbol, possibly compound and with subscripts */
{
    mpdm_t v = NULL;

    if (c->token == T_BAREWORD) {
        v = c->val;
        next_token(c);

        v = INS1(L"LIST", INS1(L"LITERAL", v));

        while (!c->error) {
            if (c->token == T_DOT) {
                /* component */
                next_token(c);

                if (c->token == T_BAREWORD) {
                    v = INS2(L"LIST", INS1(L"LITERAL", c->val), v);
                    next_token(c);
                }
                else
                    c_error(c);
            }
            else
            if (c->token == T_OBRACK) {
                /* subscript */
                next_token(c);

                v = INS2(L"LIST", expr(c), v);

                if (c->token == T_CBRACK)
                    next_token(c);
                else
                    c_error(c);
            }
            else
                break;
        }
    }

    return v;
}


static mpdm_t list(struct mpsl_a *c)
/* parses a list of comma-separated expressions */
{
    mpdm_t v = NULL;

    v = INS1(L"LIST", expr(c));

    while (!c->error && c->token == T_COMMA) {
        next_token(c);
        v = INS2(L"LIST", expr(c), v);
    }

    return v;
}


static mpdm_t object(struct mpsl_a *c, mpdm_t h)
/* parses an object */
{
    mpdm_t v = NULL;
    mpdm_t i;

    if (c->error) {}
    else
    if (c->token == T_BAREWORD) {
        i = c->val;
        next_token(c);

        if (c->token == T_COLON) {
            next_token(c);

            v = expr(c);

            if (!c->error) {
                if (h)
                    v = INS3(L"OBJECT", INS1(L"LITERAL", i), v, h);
                else
                    v = INS2(L"OBJECT", INS1(L"LITERAL", i), v);

                if (c->token == T_COMMA) {
                    next_token(c);

                    v = object(c, v);
                }
                else
                if (c->token != T_CCURLY)
                    c_error(c);
            }
        }
        else
            c_error(c);
    }
    else {
        i = expr(c);

        if (c->token == T_COLON || c->token == T_FATARRW) {
            next_token(c);

            v = expr(c);

            if (!c->error) {
                if (h)
                    v = INS3(L"OBJECT", i, v, h);
                else
                    v = INS2(L"OBJECT", i, v);

                if (c->token == T_COMMA) {
                    next_token(c);

                    v = object(c, v);
                }
                else
                if (c->token != T_CCURLY)
                    c_error(c);
            }
        }
        else
            c_error(c);
    }

    return v;
}


static wchar_t *token_to_wop(mpsl_token_t t)
{
    int n;
    mpsl_token_t tokens[] = {
        T_PLUSEQ, T_MINUSEQ, T_ASTEREQ, T_SLASHEQ, T_PERCEQ,
        T_PIPEEQ,
        T_ERROR
    };
    wchar_t *wops[] = {
        L"ADD", L"SUB", L"MUL", L"DIV", L"MOD",
        L"OR",
        NULL
    };

    for (n = 0; tokens[n] != t && wops[n]; n++);

    return wops[n];
}


static mpdm_t expr_p(struct mpsl_a *c, mpsl_op_t p_op)
/* returns an expression, using precedence */
{
    mpdm_t v = NULL;
    wchar_t *wop;

    if (c->error) {}
    else {
        if ((v = symbol(c))) {
            switch (c->token) {
            case T_EQ:
                /* assignation */
                next_token(c);
                v = INS2(L"ASSIGN", v, expr(c));
                break;

            case T_OPAREN:
                /* function call */
                next_token(c);

                if (c->token == T_CPAREN) {
                    /* function call without arguments */
                    v = INS2(L"EXECSYM", v, code_pos(c));
                    next_token(c);
                }
                else {
                    /* function call with arguments */
                    v = INS3(L"EXECSYM", v, code_pos(c), list(c));

                    if (c->token != T_CPAREN)
                        c_error(c);
                    else
                        next_token(c);
                }

                break;

            default:
                if ((wop = token_to_wop(c->token))) {
                    /* immediate operation (+=, -=...) */
                    next_token(c);
                    v = INS2(L"ASSIGN", v,
                        INS2(wop, INS1(L"SYMVAL", v), expr(c)));
                }
                else {
                    /* symbol value */
                    v = INS1(L"SYMVAL", v);
                }

                break;
            }
        }
        else
            v = term(c);

        if (v) {
            mpsl_op_t op;

            while (!c->error && (op = op_by_token(c)) && op < p_op) {
                next_token(c);

                switch (op) {
                case O_ADD: v = INS2(L"ADD", v, expr_p(c, op)); break;
                case O_SUB: v = INS2(L"SUB", v, expr_p(c, op)); break;
                case O_MUL: v = INS2(L"MUL", v, expr_p(c, op)); break;
                case O_DIV: v = INS2(L"DIV", v, expr_p(c, op)); break;
                case O_MOD: v = INS2(L"MOD", v, expr_p(c, op)); break;

                case O_EQ:  v = INS2(L"EQ",  v, expr_p(c, op)); break;
                case O_GT:  v = INS2(L"GT",  v, expr_p(c, op)); break;
                case O_GE:  v = INS2(L"GE",  v, expr_p(c, op)); break;
                case O_LT:  v = INS2(L"LT",  v, expr_p(c, op)); break;
                case O_LE:  v = INS2(L"LE",  v, expr_p(c, op)); break;
    
                case O_AND: v = INS2(L"AND", v, expr_p(c, op)); break;
                case O_OR:  v = INS2(L"OR",  v, expr_p(c, op)); break;
                case O_NE:  v = INS1(L"NOT", INS2(L"EQ", v, expr_p(c, op))); break;

                case O_FMT: v = INS2(L"FMT", v, expr_p(c, op)); break;

                case O_METHOD:
                    if (c->token == T_BAREWORD) {
                        mpdm_t w = symbol(c);

                        if (w && !c->error && c->token == T_OPAREN) {
                            next_token(c);

                            if (c->token == T_CPAREN) {
                                /* method call with no arguments */
                                next_token(c);

                                v = INS3(L"METHOD", w, code_pos(c), INS1(L"ILIST", v));
                            }
                            else {
                                /* method call with arguments */
                                mpdm_t a = list(c);

                                if (a && !c->error && c->token == T_CPAREN) {
                                    next_token(c);

                                    v = INS3(L"METHOD", w, code_pos(c), INS2(L"ILIST", v, a));
                                }
                            }
                        }
                        else
                            c_error(c);
                    }
                    else
                        c_error(c);

                    break;

                default:
                    c_error(c);
                    break;
                }
            }
        }
        else
            c_error(c);
    }

    return v;
}


static mpdm_t expr(struct mpsl_a *c)
/* returns an expression with the lower precedence */
{
    return expr_p(c, O_LOWEST);
}


static mpdm_t statement_list(struct mpsl_a *c, int sf)
{
    mpdm_t v;

    if (c->token == T_OCURLY) {
        /* code block */
        next_token(c);

        v = statement(c);

        /* concatenate statements until the end of the block */
        while (!c->error && c->token != T_EOP && c->token != T_CCURLY)
            v = INS2(L"MULTI", v, statement(c));

        if (c->token == T_CCURLY)
            next_token(c);
        else {
            c_error(c);
            v = mpdm_void(v);
        }

        /* wrap all inside a frame */
        if (v)
            v = INS1(sf ? L"SUBFRAME" : L"BLKFRAME", v);
    }
    else {
        v = statement(c);
    }

    return v;
}


static mpdm_t statement(struct mpsl_a *c)
/* returns a statement */
{
    mpdm_t v = NULL;
    mpdm_t a1 = NULL;

    if (c->error) {}
    else
    if (c->token == T_WHILE) {
        /* while clause */
        next_token(c);

        if (c->token == T_OPAREN) {
            next_token(c);

            /* pick condition */
            v = expr(c);

            if (c->token == T_CPAREN) {
                /* pick statement */
                next_token(c);

                a1 = statement_list(c, 0);

                v = INS2(L"WHILE", v, a1);
            }
            else
                c_error(c);
        }
        else
            c_error(c);
    }
    else
    if (c->token == T_FOR) {
        /* for clause */
        next_token(c);

        if (c->token == T_OPAREN) {
            next_token(c);

            /* a semicolon? it's for (;;), the infinite loop */
            if (c->token == T_SEMI) {
                next_token(c);

                if (c->token == T_SEMI) {
                    next_token(c);

                    if (c->token == T_CPAREN) {
                        /* pick statement list */
                        next_token(c);

                        v = statement_list(c, 0);

                        v = INS2(L"WHILE", INS1(L"LITERAL", mpdm_bool(1)), v);
                    }
                    else
                        c_error(c);
                }
                else
                    c_error(c);
            }
            else {
                /* real for, with init, cond, end and code */

                /* pick init expression */
                a1 = expr(c);

                if (c->token == T_SEMI) {
                    mpdm_t a2;

                    next_token(c);

                    /* pick condition */
                    a2 = expr(c);

                    if (c->token == T_SEMI) {
                        mpdm_t a3;

                        next_token(c);

                        /* final expression */
                        a3 = expr(c);

                        if (c->token == T_CPAREN) {
                            /* pick statement list */
                            next_token(c);

                            v = statement_list(c, 0);

                            v = INS4(L"WHILE", a2, v, a1, a3);
                        }
                        else {
                            c_error(c);
                            a3 = mpdm_void(a3);
                            a2 = mpdm_void(a2);
                            a1 = mpdm_void(a1);
                        }
                    }
                    else {
                        c_error(c);
                        a2 = mpdm_void(a2);
                        a1 = mpdm_void(a1);
                    }
                }
                else {
                    c_error(c);
                    a1 = mpdm_void(a1);
                }
            }
        }
        else
            c_error(c);
    }
    else
    if (c->token == T_FOREACH) {
        /* foreach loop */
        next_token(c);

        if (c->token == T_OPAREN) {
            next_token(c);

            a1 = symbol(c);

            if (a1 && !c->error) {
                if (c->token == T_COMMA) {
                    mpdm_t a2;

                    next_token(c);

                    /* FIXME */
                    /* ACHTUNG: this is an expression in
                       foreach (v, [s]), but a symbol in
                       foreach (v, [i], s) */
                    a2 = expr(c);

                    if (a2 && !c->error) {
                        if (c->token == T_CPAREN) {
                            /* foreach (a1, a2) v; */
                            next_token(c);

                            v = statement_list(c, 0);

                            if (v) {
                                v = INS3(L"FOREACH", a1, a2, v);
                                a1 = INS1(L"LOCAL", a1);

                                v = INS2(L"MULTI", a1, v);
                                v = INS1(L"BLKFRAME", v);
                            }
                            else {
                                a2 = mpdm_void(a2);
                                a1 = mpdm_void(a1);
                            }
                        }
                        else
                        if (c->token == T_COMMA) {
                            /* another argument: it may be a foreach (v, i, s) */
                            mpdm_t a3;

                            /* FIXME */
                            /* regarding the ACHTUNG above; if syntax
                               is correct, a2 must be a SYMVAL(LIST(LITERAL()));
                               get the first arg and set a2 to
                               LIST(LITERAL()) -- looking for problems */
                            a3 = mpdm_ref(mpdm_get_i(a2, 1));
                            mpdm_unref(a2);
                            a2 = a3;

                            next_token(c);

                            a3 = expr(c);

                            if (a3 && !c->error) {
                                if (c->token == T_CPAREN) {
                                    /* foreach (a1, a3, a2) v; */
                                    next_token(c);

                                    v = statement_list(c, 0);

                                    if (v) {
                                        v = INS4(L"FOREACH", a1, a3, v, a2);
                                        a2 = INS1(L"LOCAL", a2);

                                        v = INS2(L"MULTI", a2, v);
                                        a1 = INS1(L"LOCAL", a1);

                                        v = INS2(L"MULTI", a1, v);
                                        v = INS1(L"BLKFRAME", v);
                                    }
                                    else {
                                        a3 = mpdm_void(a3);
                                        a2 = mpdm_void(a2);
                                        a1 = mpdm_void(a1);
                                    }
                                }
                                else {
                                    c_error(c);
                                    a3 = mpdm_void(a3);
                                    a2 = mpdm_void(a2);
                                    a1 = mpdm_void(a1);
                                }
                            }
                            else {
                                a3 = mpdm_void(a3);
                                a2 = mpdm_void(a2);
                                a1 = mpdm_void(a1);
                            }
                        }
                        else {
                            c_error(c);
                            a2 = mpdm_void(a2);
                            a1 = mpdm_void(a1);
                        }
                    }
                    else
                        a1 = mpdm_void(a1);
                }
                else {
                    c_error(c);
                    a1 = mpdm_void(a1);
                }
            }
        }
        else
            c_error(c);
    }
    else
    if (c->token == T_BREAK) {
        next_token(c);

        /* break (exit from loops) */
        if (c->token == T_SEMI) {
            v = INS0(L"BREAK");
            next_token(c);
        }
        else
            c_error(c);
    }
    else
    if (c->token == T_RETURN) {
        next_token(c);

        /* return from subroutine (with optional argument) */
        if (c->token == T_SEMI) {
            v = INS0(L"RETURN");
            next_token(c);
        }
        else
        /* return expression */
        if ((a1 = expr(c))) {
            if (c->token == T_SEMI) {
                v = INS1(L"RETURN", a1);
                next_token(c);
            }
            else
                c_error(c);
        }
        else
            c_error(c);
    }
    else
    if (c->token == T_IF) {
        /* if clause, with optional else */
        next_token(c);

        if (c->token == T_OPAREN) {
            next_token(c);

            /* pick condition */
            v = expr(c);

            if (c->token == T_CPAREN) {
                /* pick statement */
                next_token(c);

                a1 = statement_list(c, 0);

                if (c->token == T_ELSE) {
                    mpdm_t x;

                    /* if-else */
                    next_token(c);

                    x = statement_list(c, 0);

                    v = INS3(L"IF", v, a1, x);
                }
                else
                    v = INS2(L"IF", v, a1);
            }
            else
                c_error(c);
        }
        else
            c_error(c);
    }
    else
    if (c->token == T_SUB) {
        /* subroutine definition */
        next_token(c);

        /* pick symbol name */
        a1 = symbol(c);

        if (a1 && !c->error) {
            if (c->token == T_OPAREN) {
                /* subroutine definition with parens */
                next_token(c);

                if (c->token == T_CPAREN) {
                    /* subroutine definition with empty argument list */
                    next_token(c);

                    if (c->token == T_OCURLY) {
                        v = statement_list(c, 1);
    
                        if (v && !c->error)
                            v = INS1(L"LITERAL", MPDM_X2(mpsl_exec_p, v));
                    }
                    else
                        c_error(c);
                }
                else {
                    /* subroutine definition with argument list */
                    mpdm_t a = bareword_list(c);

                    if (a && !c->error && c->token == T_CPAREN) {
                        next_token(c);

                        if (c->token == T_OCURLY) {
                            v = statement_list(c, 1);

                            if (v && !c->error) {
                                mpdm_push(v, a);
                                v = INS1(L"LITERAL", MPDM_X2(mpsl_exec_p, v));
                            }
                        }
                        else
                        c_error(c);
                    }
                }
            }
            else {
                /* subroutine definition without parens */
                if (c->token == T_OCURLY) {
                    v = statement_list(c, 1);

                    if (v && !c->error)
                        v = INS1(L"LITERAL", MPDM_X2(mpsl_exec_p, v));
                }
                else
                    c_error(c);
            }

            if (v && !c->error)
                v = INS2(L"ASSIGN", a1, v);
            else
                v = mpdm_void(v);
        }
    }
    else
    if (c->token == T_LOCAL || c->token == T_GLOBAL) {
        /* local or global symbol definitions */
        mpsl_token_t t = c->token;

        next_token(c);

        for (;;) {
            if (c->token == T_BAREWORD) {
                mpdm_t l, w;

                l = INS1(L"LITERAL", c->val);
                w = INS1(t == T_LOCAL ? L"LOCAL" : L"GLOBAL", l);

                if (v)
                    v = INS2(L"MULTI", v, w);
                else
                    v = w;

                next_token(c);

                if (c->token == T_SEMI) {
                    next_token(c);
                    break;
                }
                else
                if (c->token == T_EQ) {
                    /* variable definition with assignation */
                    next_token(c);

                    w = INS2(L"ASSIGN", l, expr(c));
                    v = INS2(L"MULTI", v, w);

                    if (c->token == T_SEMI) {
                        next_token(c);
                        break;
                    }
                    else
                    if (c->token == T_COMMA) {
                        next_token(c);
                    }
                    else {
                        c_error(c);
                        break;
                    }
                }
                else
                if (c->token == T_COMMA) {
                    next_token(c);
                }
                else {
                    c_error(c);
                    break;
                }
            }
            else {
                c_error(c);
                break;
            }
        }

        if (c->error)
            v = mpdm_void(v);
    }
    else
    if (c->token == T_SEMI) {
        /* nop instruction */
        v = INS0(L"MULTI");
        next_token(c);
    }
    else {
        /* expression */
        v = expr(c);

        if (c->token == T_SEMI)
            next_token(c);
        else {
            c_error(c);
            v = mpdm_void(v);
        }
    }

    return v;
}


static mpdm_t mpsl_a_compiler(mpdm_t code, mpdm_t source)
/* the new ad-hoc compiler entry point */
{
    mpdm_t v = NULL;
    struct mpsl_a c;

    mpdm_ref(code);

    /* reset compiler structure */
    memset(&c, '\0', sizeof(c));

    mpdm_store(&c.source, source);

    /* store the source (file or string) */
    if (mpdm_type(code) == MPDM_TYPE_FILE)
        c.f = mpdm_get_filehandle(code);
    else
        c.ptr = mpdm_string(code);

    nc(&c);

    /* does it start with a she-bang? */
    if (c.c == L'#') {
        if (nc(&c) == L'!') {
            /* yes; drop first line */
            while (nc(&c) != L'\n' && c.c != WEOF);
        }
    }

    next_token(&c);
    v = statement(&c);

    while (!c.error && c.token != T_EOP)
        v = INS2(L"MULTI", v, statement(&c));

    /* if there is no error, create a program */
    if (!c.error && v)
        v = mpsl_x(v, NULL, 1);
    else
        v = mpdm_void(v);

    mpdm_store(&c.source, NULL);

    mpdm_unref(code);

    return v;
}


mpdm_t mpsl_a_compiler_x(mpdm_t args, mpdm_t ctxt)
{
    return mpsl_a_compiler(mpdm_get_i(args, 0), mpdm_get_i(args, 1));
}
