/*

    MPSL - Minimum Profit Scripting Language

    ttcdt <dev@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "mpdm.h"
#include "mpsl.h"

/* total number of tests and oks */
int tests = 0;
int oks = 0;

/* failed tests messages */
char *failed_msgs[5000];
int i_failed_msgs = 0;

/*******************
    Code
********************/

#define do_test(s, o) _do_test(s, o ,__LINE__)

void _do_test(char *str, int ok, int line)
{
    char tmp[1024];

    snprintf(tmp, sizeof(tmp), "mpsl-stress.c:%d: %s: %s\n", line, str, ok ? "OK!" : "*** Failed ***");
    printf("%s", tmp);

    tests++;

    if (ok)
        oks++;
    else
        failed_msgs[i_failed_msgs++] = strdup(tmp);
}


/* tests */

void do_set(mpdm_t * v1, mpdm_t v2)
{
    mpdm_ref(v2);
    mpdm_unref(*v1);
    *v1 = v2;
}


#define do_test_mpsl(c) _do_test_mpsl(c, __LINE__)

mpdm_t _do_test_mpsl(char *code, int line)
{
    static mpdm_t v = NULL;

    do_set(&v, mpsl_compile(MPDM_MBS(code), NULL));

    printf("Compile: ");
    _do_test(code, v != NULL, line);
    return (v);
}


mpdm_t do_test_mpsl_file(char *file, mpdm_t inc)
{
    static mpdm_t v = NULL;

    do_set(&v, mpsl_compile_file(MPDM_MBS(file), inc));

    printf("Compile file: ");
    do_test(file, v != NULL);
    return (v);
}


mpdm_t do_test_exec(mpdm_t x, mpdm_t a)
{
    static mpdm_t v = NULL;

    do_set(&v, mpdm_exec(x, a, NULL));

    return (v);
}


void test_mpsl(void)
{
    mpdm_t v;

    printf("MPSL (Minimum Profit Scripting Language)\n\n");

    v = mpsl_compile(MPDM_S(L"a=1;"), NULL);
    do_test_exec(v, NULL);

    printf("MPSL compilation tests-------------\n");

    do_test_mpsl("a=1;");
    do_test_mpsl("a.b.c=1;");
    do_test_mpsl("a.b.c=d;");
    do_test_mpsl("a.b.c=d.e.f;");
    do_test_mpsl("a[\"b\"]=1;");
    do_test_mpsl("a[\"b\"].c=1;");
    do_test_mpsl("a[\"b\"][\"c\"]=1;");
    do_test_mpsl("/* empty hash */ days={};");
    do_test_mpsl("days.lunes=\"monday\";");
    do_test_mpsl("days[\"martes\"]=\"tuesday\";");

    do_test_mpsl("1 + ((3 - 5) * 8);");
    do_test_mpsl("1.5 + ((3.1 - 5.8) * 8.0);");
    do_test_mpsl("a=1 + ((3 - 5) * 8);");
    do_test_mpsl("2 + 3 * 4;");
    do_test_mpsl("2 * 3 + 4;");

    do_test_mpsl("/* hash */ y={ 'enero' => 'january', 'febrero' => 'february' };");
    do_test_mpsl("/* array */ a=[\"this\", \"one\", \"is\", 666, \"cool\"];");

    do_test_mpsl
        ("/* greatest common divisor (Euclid's algorithm) */ sub gcd(m, n) { while (m > 0) { if(n > m) { local t = m; m = n; n = t; } m -= n; } n; }");

//    do_test_mpsl("/* range assign */ a = [ 1 .. 1000 ];");

//    do_test_mpsl("a.b.c ++;");
//    do_test_mpsl("a.b.c --;");
    do_test_mpsl("a.b.c += 100;");

//    do_test_mpsl("foreach (a, [ 1 .. 1000 ]) { print(a); }");

    do_test_mpsl("local a;");
    do_test_mpsl("local a, b, c;");
    do_test_mpsl("local a = 666;");
    do_test_mpsl("local a; a = 666;");

    do_test_mpsl("a > b - 1;");
    do_test_mpsl("a > b - 1 && a < c + 1;");

    do_test_mpsl("a = NULL;");
    do_test_mpsl("a = 100; b = 200;c = 300;");
    do_test_mpsl("sub test {a = 100; b = 200;c = 300;}");
    do_test_mpsl("sub test (d, e) {a = 100; b = 200;c = b;}");
    do_test_mpsl("a();");
    do_test_mpsl("! 1 > 2;");
    do_test_mpsl("! (1 > 2);");
    do_test_mpsl("1 != 2;");
    do_test_mpsl("\"hello\" != \"goodbye\";");

    mpdm_dump(do_test_mpsl("sub test(a, b) { c=1; }"));
    mpdm_dump(do_test_mpsl("sub test(a, b) { c=1; d=2; }"));
    mpdm_dump(do_test_mpsl("sub test(a, b) { c=1; d=2; e=3; }"));
}


void test_mpsl2(void)
{
    mpdm_t v;
    mpdm_t w;

    /* execution tests */
    v = do_test_mpsl("666;");
    mpdm_dump(v);
    v = do_test_exec(v, NULL);
    do_test("literal number", mpdm_ival(v) == 666);

    v = do_test_mpsl("\"goodbye\";");
    v = do_test_exec(v, NULL);
    do_test("literal string", mpdm_cmp(v, MPDM_S(L"goodbye")) == 0);

    v = do_test_mpsl("1 + 3 + 5;");
    v = do_test_exec(v, NULL);
    do_test("mpsl calculator 1", mpdm_rval(v) == 9.0);

    v = do_test_mpsl("1 + ((3 - 5) * 8);");
    v = do_test_exec(v, NULL);
    do_test("mpsl calculator 2", mpdm_rval(v) == -15.0);

    /* next value cannot be tested as an exact equality,
       as rounding errors will manifest */
    v = do_test_mpsl("1.5 + ((3.1 - 5.8) * 8.0);");
    v = do_test_exec(v, NULL);
    do_test("mpsl calculator 3", mpdm_rval(v) < -20.0 && mpdm_rval(v) > -21.0);

    v = do_test_mpsl("2 + 3 * 4;");
    v = do_test_exec(v, NULL);
    do_test("mpsl calculator 4", mpdm_rval(v) == 14.0);

    v = do_test_mpsl("2 * 3 + 4;");
    v = do_test_exec(v, NULL);
    do_test("mpsl calculator 5", mpdm_rval(v) == 10.0);

    v = do_test_exec(do_test_mpsl("2 + 3 * 4;"), NULL);
    mpdm_ref(v);
    w = do_test_exec(do_test_mpsl("2 + (3 * 4);"), NULL);
    do_test("mpsl calculator 6 (operator precedence)", mpdm_rval(v) == mpdm_rval(w));
    mpdm_unref(v);

    v = do_test_exec(do_test_mpsl("2 + 3 * 4;"), NULL);
    mpdm_ref(v);
    w = do_test_exec(do_test_mpsl("(2 + 3) * 4;"), NULL);
    do_test("mpsl calculator 7 (operator precedence)", mpdm_rval(v) != mpdm_rval(w));
    mpdm_unref(v);

    v = do_test_mpsl("/* array */ [\"this\", \"one\", \"is\", 666, \"cool\"];");
    v = do_test_exec(v, NULL);
    mpdm_dump(v);
    do_test("mpsl array", mpdm_ival(mpdm_get_i(v, 3)) == 666);

    v = do_test_mpsl
        ("/* hash */ { \"enero\" => \"january\", \"febrero\" => \"february\" };");
    v = do_test_exec(v, NULL);
    mpdm_dump(v);
    do_test("mpsl hash", mpdm_cmp(mpdm_get(v,
                        MPDM_S(L"febrero")),
                      MPDM_S(L"february")) == 0);

    v = do_test_mpsl("! 1;");
    v = do_test_exec(v, NULL);
    do_test("boolean not 1", !mpdm_is_true(v));
    v = do_test_mpsl("! 0;");
    v = do_test_exec(v, NULL);
    do_test("boolean not 2", v != NULL);

    v = do_test_mpsl("1 && 3;");
    v = do_test_exec(v, NULL);
    do_test("boolean and 1", mpdm_ival(v) == 3);
    v = do_test_mpsl("1 && 0;");
    v = do_test_exec(v, NULL);
    do_test("boolean and 2", !mpdm_is_true(v));
    v = do_test_mpsl("0 && 1;");
    v = do_test_exec(v, NULL);
    do_test("boolean and 3", !mpdm_is_true(v));

    v = do_test_mpsl("1 || 3;");
    v = do_test_exec(v, NULL);
    do_test("boolean or 1", mpdm_ival(v) == 1);
    v = do_test_mpsl("2 || 0;");
    v = do_test_exec(v, NULL);
    do_test("boolean or 2", mpdm_ival(v) == 2);
    v = do_test_mpsl("0 || 3;");
    v = do_test_exec(v, NULL);
    do_test("boolean or 3", mpdm_ival(v) == 3);

    v = do_test_mpsl("6 == 6;");
    v = do_test_exec(v, NULL);
    do_test("numeric == 1", v != NULL);
    v = do_test_mpsl("8.0 == 8.0;");
    v = do_test_exec(v, NULL);
    do_test("numeric == 2", v != NULL);
    v = do_test_mpsl("6 == 8;");
    v = do_test_exec(v, NULL);
    do_test("numeric == 3", !mpdm_is_true(v));

    v = do_test_mpsl("6 != 6;");
    v = do_test_exec(v, NULL);
    do_test("numeric != 1", !mpdm_is_true(v));
    v = do_test_mpsl("8.0 != 8.0;");
    v = do_test_exec(v, NULL);
    do_test("numeric != 2", !mpdm_is_true(v));
    v = do_test_mpsl("6 != 8;");
    v = do_test_exec(v, NULL);
    do_test("numeric != 3", v != NULL);

    v = do_test_mpsl("6 < 6;");
    v = do_test_exec(v, NULL);
    do_test("numeric < 1", !mpdm_is_true(v));
    v = do_test_mpsl("8 < 6;");
    v = do_test_exec(v, NULL);
    do_test("numeric < 2", !mpdm_is_true(v));
    v = do_test_mpsl("5 < 6;");
    v = do_test_exec(v, NULL);
    do_test("numeric < 3", v != NULL);

    v = do_test_mpsl("6 > 6;");
    v = do_test_exec(v, NULL);
    do_test("numeric > 1", !mpdm_is_true(v));
    v = do_test_mpsl("8 > 6;");
    v = do_test_exec(v, NULL);
    do_test("numeric > 2", v != NULL);
    v = do_test_mpsl("5 > 6;");
    v = do_test_exec(v, NULL);
    do_test("numeric > 3", !mpdm_is_true(v));

    v = do_test_mpsl("6 <= 6;");
    v = do_test_exec(v, NULL);
    do_test("numeric <= 1", v != NULL);
    v = do_test_mpsl("8 <= 6;");
    v = do_test_exec(v, NULL);
    do_test("numeric <= 2", !mpdm_is_true(v));
    v = do_test_mpsl("5 <= 6;");
    v = do_test_exec(v, NULL);
    do_test("numeric <= 3", v != NULL);

    v = do_test_mpsl("6 >= 6;");
    v = do_test_exec(v, NULL);
    do_test("numeric >= 1", v != NULL);
    v = do_test_mpsl("8 >= 6;");
    v = do_test_exec(v, NULL);
    do_test("numeric >= 2", v != NULL);
    v = do_test_mpsl("5 >= 6;");
    v = do_test_exec(v, NULL);
    do_test("numeric >= 3", !mpdm_is_true(v));

    v = do_test_mpsl("11 % 6;");
    v = do_test_exec(v, NULL);
    do_test("modulo", mpdm_ival(v) == 5);

    v = do_test_mpsl("variable=16384;");
    mpdm_dump(v);
    v = do_test_exec(v, NULL);
    do_test("assign 1", mpdm_ival(v) == 16384);

    v = do_test_mpsl("array=[10, 20, 30, 40];");
    v = do_test_exec(v, NULL);
    do_test("assign 2", mpdm_ival(mpdm_get_i(v, 2)) == 30);

    v = do_test_mpsl("a=1; b=2; c=3;");
    mpdm_dump(v);
    v = do_test_exec(v, NULL);

    v = do_test_mpsl("CACHE={}; CACHE.regex=[]; CACHE.regex[0]=12345;");
    v = do_test_exec(v, NULL);

    v = do_test_mpsl("variable;");
    v = do_test_exec(v, NULL);
    do_test("symval 1", mpdm_ival(v) == 16384);

    v = do_test_mpsl("variable2=1 + ((3 - 5) * 8); variable2;");
    mpdm_dump(v);
    v = do_test_exec(v, NULL);
    do_test("symval 2", mpdm_rval(v) == -15);

    v = do_test_mpsl("variable3=variable2 * 2;");
    v = do_test_exec(v, NULL);
    do_test("symval 3", mpdm_ival(v) == -30);

    v = do_test_mpsl("sub mysum(a, b) { a + b; }");
    mpdm_dump(v);
    v = do_test_exec(v, NULL);
    do_test("sub 1", v != NULL);

    v = do_test_mpsl("sub pi() { 3.1416; }");
    mpdm_dump(v);
    v = do_test_exec(v, NULL);
    do_test("sub 2", v != NULL);

    v = do_test_mpsl("var10=pi();");
    v = do_test_exec(v, NULL);
    do_test("exec 1", mpdm_rval(v) == 3.1416);

    v = do_test_mpsl("var11=pi() * 10000; var11;");
    v = do_test_exec(v, NULL);
    do_test("exec 2", mpdm_rval(v) == 31416);

    v = do_test_mpsl("mysum(100, 20);");
    v = do_test_exec(v, NULL);
    do_test("exec 3", mpdm_rval(v) == 120.0);

    v = do_test_mpsl("a = NULL;");
    v = do_test_exec(v, NULL);
    do_test("NULL 1", v == NULL);

    v = do_test_mpsl("a == NULL;");
    v = do_test_exec(v, NULL);
    do_test("NULL 2", mpdm_ival(v) == 1);

    v = do_test_mpsl("local a, b; a = 1; b = 2;");
    v = do_test_exec(v, NULL);

    v = do_test_mpsl("a == NULL;");
    v = do_test_exec(v, NULL);
    do_test("local 1", mpdm_ival(v) == 1);

    v = do_test_mpsl("66 * -1;");
    v = do_test_exec(v, NULL);
    do_test("uminus", mpdm_ival(v) == -66);

    v = do_test_mpsl("\"test\" == \"test\";");
    v = do_test_exec(v, NULL);
    do_test("streq 1", mpdm_is_true(v));

    v = do_test_mpsl("\"test\" == \"prueba\";");
    v = do_test_exec(v, NULL);
    do_test("streq 1", !mpdm_is_true(v));

//    v = do_test_mpsl("a = 6; ++ a;");
    v = do_test_exec(v, NULL);
//    do_test("pinc", mpdm_ival(v) == 7);

//    v = do_test_mpsl("a++;");
    v = do_test_exec(v, NULL);
//    do_test("sinc", mpdm_ival(v) == 7);

    v = do_test_mpsl("a += 10;");
    v = do_test_exec(v, NULL);
//    do_test("iadd", mpdm_ival(v) == 18);

    v = do_test_mpsl("local a, b, c; a=1; b=2; c=3; if(a == b) c=1000; c;");
    v = do_test_exec(v, NULL);
    do_test("if 1", mpdm_ival(v) == 3);

    v = do_test_mpsl("local a, b, c; a=1; b=2; c=3; if(a <= b) c=1000; c;");
    v = do_test_exec(v, NULL);
    do_test("if 2", mpdm_ival(v) == 1000);

    v = do_test_mpsl("local a, b, c; a=1; b=2; if(a == b) c=1000; else c=2000; c;");
    v = do_test_exec(v, NULL);
    do_test("ifelse", mpdm_ival(v) == 2000);

//    v = do_test_mpsl("local a; a = 0; while(a < 100) { a++; } a;");
    v = do_test_exec(v, NULL);
//    do_test("ifelse", mpdm_ival(v) == 100);

    v = do_test_mpsl("a=mysum(100, 50); a;");
    v = do_test_exec(v, NULL);
    do_test("mysum 1", mpdm_ival(v) == 150);

    v = do_test_mpsl("a=mysum(2000, 500); a;");
    v = do_test_exec(v, NULL);
    do_test("mysum 2", mpdm_ival(v) == 2500);

    w = mpdm_ref(MPDM_A(2));
    mpdm_set_i(w, MPDM_I(100), 0);
    mpdm_set_i(w, MPDM_I(50), 1);

    /* asks for the value of the mysum symbol (the code) */
    v = do_test_mpsl("mysum;");
    /* executes, so mysum() itself is being returned */
    v = do_test_exec(v, NULL);
    mpdm_dump(v);
    do_test("mysum 3", mpdm_ival(do_test_exec(v, w)) == 150);

    mpdm_set_i(w, MPDM_I(75), 1);
    do_test("mysum 4", mpdm_ival(do_test_exec(v, w)) == 175);

    /* compiles (and executes) the definition of gcd() */
    v = do_test_mpsl
        ("/* greatest common divisor (Euclid's algorithm) */ sub gcd(m, n) { while (m > 0) { if(n > m) { local t = m; m = n; n = t; } m -= n; } n; }");
    do_test_exec(v, NULL);

    /* gets a pointer to gcd() */
    v = do_test_exec(do_test_mpsl("gcd;"), NULL);
    mpdm_dump(v);

    /* executes gcd(100, 50); */
    mpdm_set_i(w, MPDM_I(50), 1);
    do_test("gcd() 1", mpdm_ival(do_test_exec(v, w)) == 50);

    /* executes gcd(100, 75); */
    mpdm_set_i(w, MPDM_I(75), 1);
    do_test("gcd() 2", mpdm_ival(do_test_exec(v, w)) == 25);

    mpdm_unref(w);

    /* string concatenation */
    w = mpdm_ref(MPDM_S(L"big lebowski"));

    v = do_test_mpsl("\"big\" + \" lebowski\";");
    do_test("~ (strcat 1)", mpdm_cmp(do_test_exec(v, NULL), w) == 0);

    v = do_test_mpsl("\"big\" + \" \" + \"lebowski\";");
    do_test("~ (strcat 2)", mpdm_cmp(do_test_exec(v, NULL), w) == 0);

    mpdm_unref(w);
}


void test_mpsl3(void)
{
    mpdm_t v;

    v = do_test_mpsl("v=[10,20]; w=v[0]; w;");
    mpdm_dump(v);
    v = do_test_exec(v, NULL);
    mpdm_dump(v);

    /* library functions tests */
    v = do_test_mpsl("dump( [1, 2, 3, 4, 5] );");
    do_test_exec(v, NULL);

    v = do_test_mpsl("if(size([2, 3, 4]) == 3) { dump(\"YES\"); } else { dump(\"NO\"); }");
    do_test_exec(v, NULL);

//    v = do_test_mpsl("is_array(1);");
//    do_test("is_array 1", !mpdm_is_true(do_test_exec(v, NULL)));
//    v = do_test_mpsl("is_array([]);");
//    do_test("is_array 2", do_test_exec(v, NULL) != NULL);
//    v = do_test_mpsl("is_array({});");
//    do_test("is_array 3", do_test_exec(v, NULL) != NULL);

//    v = do_test_mpsl("is_hash(1);");
//    do_test("is_hash 1", !mpdm_is_true(do_test_exec(v, NULL)));
//    v = do_test_mpsl("is_hash([]);");
//    do_test("is_hash 2", !mpdm_is_true(do_test_exec(v, NULL)));
//    v = do_test_mpsl("is_hash({});");
//    do_test("is_hash 3", do_test_exec(v, NULL) != NULL);


/*    v = do_test_mpsl("v=splice(\"inventions of life\", NULL, 0, 10); v[1];");
    v = do_test_exec(v, NULL);
    do_test("splice 1", mpdm_cmp(v, MPDM_S(L"inventions")) == 0);

    v = do_test_mpsl("v[0];");
    v = do_test_exec(v, NULL);
    do_test("splice 2", mpdm_cmp(v, MPDM_S(L" of life")) == 0);
*/
    v = do_test_mpsl("sub func() { if(1 == 1) { return(6); 24; } 12; }");
    v = do_test_exec(v, NULL);
    v = do_test_mpsl("a=func();");
    v = do_test_exec(v, NULL);
    do_test("return 1", mpdm_ival(v) == 6);

    v = do_test_mpsl("a=func(); 500;");
    v = do_test_exec(v, NULL);
    do_test("return 2", mpdm_ival(v) == 500);

    v = do_test_mpsl("sub func() { while(1 == 1) { return(6); 24; } 12; }");
    v = do_test_exec(v, NULL);
    v = do_test_mpsl("a=func();");
    v = do_test_exec(v, NULL);
    do_test("return 3", mpdm_ival(v) == 6);

//    v = do_test_mpsl("sub func() { foreach(a, [1 .. 10]) { return(6); 24; } 12; }");
    v = do_test_exec(v, NULL);
    v = do_test_mpsl("a=func();");
    v = do_test_exec(v, NULL);
    do_test("return 4", mpdm_ival(v) == 6);

//    v = do_test_mpsl("a=1; while(a < 10) { a++; } a;");
    v = do_test_exec(v, NULL);
//    do_test("while", mpdm_ival(v) == 10);

//    v = do_test_mpsl("a=1; while(a < 10) { if(a == 5) break; a++; } a;");
    v = do_test_exec(v, NULL);
//    do_test("break", mpdm_ival(v) == 5);

    v = do_test_mpsl("b=0; foreach(a, [1, 2, 3, 4]) { dump(a); b += a; } return(b);");
    v = do_test_exec(v, NULL);
    do_test("foreach", mpdm_ival(v) == 10);

//    v = do_test_mpsl("b=0; foreach(a, [1 .. 20]) { dump(a); b += a; } return(b);");
    v = do_test_exec(v, NULL);
//    do_test("foreach+range 1", mpdm_ival(v) == 210);

//    v = do_test_mpsl("b=0; foreach(a, [20 .. 1]) { dump(a); b += a; } return(b);");
    v = do_test_exec(v, NULL);
//    do_test("foreach+range 2", mpdm_ival(v) == 210);

    v = do_test_mpsl("print(\"print: \", 1, 2, [1, 2, 3], func(), 4);");
    v = do_test_exec(v, NULL);

    v = do_test_mpsl("'This is\\na string.';");
    v = do_test_exec(v, NULL);
    mpdm_dump(v);

    v = do_test_mpsl("\"This is\\na string.\";");
    v = do_test_exec(v, NULL);
    mpdm_dump(v);

    v = do_test_mpsl("sub t(h) { dump(h); dump(h.x); h.x; } H={}; H.x=5; t(H);");
    v = do_test_exec(v, NULL);
    do_test("Accesing a hash's component passed as argument", mpdm_ival(v) == 5);

/*  mpdm_dump(mpdm_root());*/
}


void test_abort_and_eval(void)
{
    mpdm_t v;
    mpdm_t e;

    mpsl_abort = 0;
    v = mpdm_ref(mpsl_compile(MPDM_S(L"1000;"), NULL));
    do_test("Abort 1", mpdm_ival(mpdm_exec(v, NULL, NULL)) == 1000);

    /* set global abort function */
    mpsl_abort = 1;
    do_test("Abort 2", mpdm_exec(v, NULL, NULL) == NULL);

    mpsl_abort = 0;
    do_test("Abort 3", mpdm_ival(mpdm_exec(v, NULL, NULL)) == 1000);
    mpdm_unref(v);

    mpsl_error(NULL);

    v = mpsl_eval(MPDM_S(L"invalid_code()"), NULL, NULL);
    e = mpdm_get_wcs(mpdm_root(), L"ERROR");
    printf("The following error is OK:\n");
    mpdm_dump(e);
    do_test("eval 1", v == NULL && e != NULL);

    v = mpsl_eval(MPDM_S(L"undef_func();"), NULL, NULL);
    e = mpdm_get_wcs(mpdm_root(), L"ERROR");
    printf("The following error is also OK:\n");
    mpdm_dump(e);
    do_test("eval 2", v == NULL && e != NULL);

    v = mpsl_eval(MPDM_S(L"load('unexistent_file.mpsl');"), NULL, NULL);
    e = mpdm_get_wcs(mpdm_root(), L"ERROR");
    printf("The following error is also OK:\n");
    mpdm_dump(e);
    do_test("eval 3", v == NULL && e != NULL);

    v = mpsl_eval(MPDM_S(L"2000;"), NULL, NULL);
    e = mpdm_get_wcs(mpdm_root(), L"ERROR");
    do_test("eval 4", mpdm_ival(v) == 2000 && e == NULL);
}


mpdm_t mpsl_a_compile(mpdm_t t);

#define CT(s) { s, __LINE__ }

void test_compiler_diffs(void)
{
    mpdm_t r, cc, ac;
    int n;
    struct {
        wchar_t *code;
        int line;
    } compiler_tests[] = {
//        CT(L"local a = 1, b = 2, c, d = 3;"),
//        CT(L"local a = 1;"),
//        CT(L"local v; 1;"),
//        CT(L"local x, y, z; 2;"),
        CT(L"a |= 6;"),
        CT(L"a %= 6;"),
        CT(L"a /= 6;"),
        CT(L"a *= 6;"),
        CT(L"a -= 6;"),
        CT(L"a += 6;"),
        CT(L"for (n = 0; n < 100; n = n + 1) { print('kill'); } 1234;"),
        CT(L"for (;;) { print('kill'); } 1234;"),
        CT(L"foreach (v, i, [1, 2, 3]) { print(e); } 666;"),
        CT(L"eol = driver == 'win32' && 'crlf' || 'lf';"),
        CT(L"255 $ '%x';"),
        CT(L"foreach (e, [1, 2, 3]) { print(e); } 666;"),
//        CT(L"global v; 1;"),
//        CT(L"global x, y, z; 2;"),
        CT(L"sub pi { 3.1416; } 100;"),
        CT(L"sub pi () { 3.1416; } 200;"),
        CT(L"sub by2(v) { v * 2; } 250;"),
        CT(L"sub mul(v1, v2) { v1 * v2; } 300;"),
        CT(L"mul = sub (v1, v2) { v1 * v2; }; 123;"),
        CT(L"by2 = sub (e) { e * 2; }; 100;"),
        CT(L"pi = sub { 3.14; }; 6;"),
        CT(L"f->write('hi', string(1 + 3), eol); 1;"),
        CT(L"f->read(); 1;"),
        CT(L"while (1) 2;"),
        CT(L"while (1) { 2; 3; }"),
        CT(L"while (a < 10) { a = a + 1; }"),
        CT(L"if (a == 1) { b = 2 + 4; c = 3 * 2; } else { d = 3; e = d / 2; }"), 
        CT(L"if (1) 2; else 3;"), 
        CT(L"if (1) { 2; 3; }"),
        CT(L"if (1) { 2; }"),
        CT(L"if (1) 2;"),
        CT(L"if (2 + 3) 4 + 5;"),
        CT(L"if (a == 1) b = 2;"),
        CT(L"list[0];"),
        CT(L"list[1] = 1;"),
        CT(L"MPSL['OPCODE'];"),
        CT(L"q = 1 + 2 * 3;"),
        CT(L"q.q = 1 * 2 + 3; q2 = [];"),
        CT(L"q = 100;"),
        CT(L"MPSL.CORE.random;"),
        CT(L"1 + 2; [1, 2] ; {};"),
        CT(L"a + 1;"),
        CT(L"1 * (2 + 3);"),
        CT(L"{};"),
        CT(L"{a: 1};"),
        CT(L"{c: 2, d: 3};"),
        CT(L"{'e' => 4, 'f' => 5};"),
        CT(L"{g: 6, 'h' => 7};"),
        CT(L"{a: 2 * 3, b: 5 + 8};"),
        CT(L"[];"),
        CT(L"[1, 2, 3];"),
        CT(L"[1, 2 + 3, 4];"),
        CT(L"MPSL.CORE.random();"),
        CT(L"bool(1, 2) + 666;"),
        CT(L"1 ; 2 ; 3;"),
        CT(L"random();"),
        CT(L"5 != '5';"),
        CT(L"10 == '10';"),
        CT(L"10 > 1 + 2;"),
        CT(L"1 + 2 * 3;"),
        CT(L"1 * 2 + 3;"),
        CT(L"1.2 + 3.4;"),
        CT(L"/* test test */"),
        CT(L"1;"),
        CT(L"!1;"),
        CT(L"'abcde';"),
        CT(L"3.14;"),
        CT(NULL)
    };

    printf("\nComparing the output of the two compilers:\n");

    r  = mpdm_get_wcs(mpdm_root(), L"MPSL");
    cc = mpdm_get_wcs(r, L"c_compiler");
    ac = mpdm_get_wcs(r, L"a_compiler");

    for (n = 0; compiler_tests[n].code; n++) {
        mpdm_t c, x1, x2, d1, d2;

        c = MPDM_S(compiler_tests[n].code);

        mpdm_ref(c);

        mpdm_set_wcs(r, cc, L"compiler");
        x1 = mpsl_compile(c, NULL);
        d1 = mpsl_decompile(x1);

        mpdm_set_wcs(r, ac, L"compiler");
        x2 = mpsl_compile(c, NULL);
        d2 = mpsl_decompile(x2);

/*        printf("%ls\n", mpdm_string(d1));
        printf("%ls\n", mpdm_string(d2));*/

        _do_test("compiler output equal",
            mpdm_cmp(d1, d2) == 0, compiler_tests[n].line);

        mpdm_unref(c);
    }

    mpdm_set_wcs(r, cc, L"compiler");
}


int main(void)
{
    mpsl_startup();

    test_mpsl();
    test_mpsl2();
    test_mpsl3();
//    test_mpsl_file();
    test_abort_and_eval();

    test_compiler_diffs();

    mpsl_shutdown();

    printf("\n*** Total tests passed: %d/%d\n", oks, tests);

    if (oks == tests)
        printf("*** ALL TESTS PASSED\n");
    else {
        int n;

        printf("*** %d %s\n", tests - oks, "TESTS ---FAILED---");

        printf("\nFailed tests:\n\n");
        for (n = 0; n < i_failed_msgs; n++)
            printf("%s", failed_msgs[n]);
    }

    return (0);
}
