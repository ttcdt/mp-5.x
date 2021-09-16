/*

    MPSL - Minimum Profit Scripting Language
    main()

    ttcdt <dev@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <unistd.h>
#include "mpdm.h"
#include "mpsl.h"


/** code **/

mpdm_t trap_func(mpdm_t args, mpdm_t ctxt)
{
    mpdm_t c = mpdm_get_i(args, 0);
    mpdm_t r = mpdm_get_i(args, 2);

    printf("-- Code ---------------\n");
    mpdm_dump(c);
    printf("-- Ret  ---------------\n");
    mpdm_dump(r);

    printf("Press ENTER\n");
    getchar();

    return NULL;
}


int mpsl_main(int argc, char *argv[])
{
    mpdm_t v = NULL;
    mpdm_t w = NULL;
    char *immscript = NULL;
    FILE *script = NULL;
    int ret = 0;
    int dump_only = 0;
    int install_trap = 0;
    int ok = 0;
    wchar_t *compiler = NULL;

    /* skip the executable */
    argv++;
    argc--;

    while (!ok && argc > 0) {
        if (strcmp(argv[0], "-v") == 0 || strcmp(argv[0], "--help") == 0) {
            printf("MPSL %s - Minimum Profit Scripting Language\n",
                   VERSION);
            printf("ttcdt <dev@triptico.com>\n");
            printf("This software is released into the public domain. NO WARRANTY.\n\n");

            printf("Usage: mpsl [-d | -s] [-cc | -ca] [-e 'script' | script.mpsl ]\n\n");

            return 0;
        }
        else
        if (strcmp(argv[0], "-d") == 0)
            dump_only = 1;
        else
        if (strcmp(argv[0], "-s") == 0)
            install_trap = 1;
        else
        if (strcmp(argv[0], "-cc") == 0)
            compiler = L"c_mpsl";
        else
        if (strcmp(argv[0], "-ca") == 0)
            compiler = L"a_mpsl";
        else
        if (strcmp(argv[0], "-e") == 0) {
            argv++;
            argc--;
            immscript = argv[0];
            ok = 1;
        }
        else {
            /* next argument is a script name; open it */
            if ((script = fopen(argv[0], "r")) == NULL) {
                fprintf(stderr, "mpsl: Can't open '%s'\n", argv[0]);
                return 1;
            }
            ok = 1;
        }

        argv++;
        argc--;
    }

    mpsl_startup();

    if (compiler) {
        mpdm_t c;
        v = mpdm_get_wcs(mpdm_root(), L"MPSL");
        c = mpdm_get_wcs(v, compiler);

        if (c)
            mpdm_set_wcs(v, c, L"compiler");
        else {
            fprintf(stderr, "mpsl: Selected compiler not found -- using default\n");
        }
    }

    /* set arguments */
    mpsl_argv(argc, argv);

    if (install_trap)
        mpsl_trap(MPDM_X(trap_func));

    /* compile */
    if (immscript != NULL) {
        w = mpdm_ref(MPDM_MBS(immscript));
        v = mpsl_compile(w, NULL);
        mpdm_unref(w);
    }
    else {
        int c;

        /* if no input file has been defined, create
           a copy of stdin */
        if (script == NULL) {
            c = dup(0);
            script = fdopen(c, "r");
        }

        /* if line starts with #!, discard it */
        if ((c = getc(script)) == '#' && (c = getc(script)) == '!')
            while ((c = getc(script)) != EOF && c != '\n');
        else
            ungetc(c, script);

        if (c != EOF) {
            w = mpdm_ref(MPDM_F(script));
            v = mpsl_compile_file(w, NULL);
            mpdm_close(w);
            mpdm_unref(w);
        }
    }

    if (v != NULL) {
        mpdm_ref(v);

        if (dump_only) {
            mpdm_write_wcs(stdout, mpdm_string(mpsl_decompile(v)));
            printf("\n");
        }
        else
            mpdm_void(mpdm_exec(v, NULL, NULL));

        mpdm_unref(v);
    }

    /* prints the error, if any */
    if ((w = mpdm_get_wcs(mpdm_root(), L"ERROR")) != NULL) {
        fprintf(stderr, "mpsl: ");
        mpdm_write_wcs(stderr, mpdm_string(w));
        fprintf(stderr, "\n");

        ret = 1;
    }

    mpsl_shutdown();

    return ret;
}


int main(int argc, char *argv[])
{
    return mpsl_main(argc, argv);
}
