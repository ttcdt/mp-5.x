/*

    MPSL - Minimum Profit Scripting Language

    ttcdt <dev@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#ifndef MPSL_H_
#define MPSL_H_

#ifdef __cplusplus
extern "C" {
#endif

extern int mpsl_abort;

extern mpdm_t mpsl_opcodes;

mpdm_t mpsl_set_symbol(mpdm_t s, mpdm_t v, mpdm_t l);
mpdm_t mpsl_get_symbol(mpdm_t s, mpdm_t l);

mpdm_t mpsl_error(mpdm_t err);

mpdm_t mpsl_exec_p(mpdm_t c, mpdm_t args, mpdm_t ctxt);
mpdm_t mpsl_mkins(wchar_t * opcode, int args, mpdm_t a1, mpdm_t a2, mpdm_t a3, mpdm_t a4);
mpdm_t mpsl_x(mpdm_t a1, mpdm_t a2, int sf);

mpdm_t mpsl_c_compiler_x(mpdm_t code, mpdm_t source);
mpdm_t mpsl_a_compiler_x(mpdm_t code, mpdm_t source);

mpdm_t mpsl_compile(mpdm_t code, mpdm_t src);
mpdm_t mpsl_compile_file(mpdm_t filename, mpdm_t inc);
mpdm_t mpsl_resource(mpdm_t file, mpdm_t inc);
mpdm_t mpsl_eval(mpdm_t code, mpdm_t args, mpdm_t ctxt);

mpdm_t mpsl_trap(mpdm_t trap_func);

void mpsl_argv(int argc, char * argv[]);
int mpsl_bootstrap(int argc, char *argv[], const char *code, int size);

int mpsl_startup(void);
void mpsl_shutdown(void);

wchar_t *mpsl_dump_1(const mpdm_t v, int l, wchar_t *ptr, int *size);

mpdm_t mpsl_decompile(mpdm_t prg);

#ifdef __cplusplus
}
#endif

#endif /* MPSL_H_ */
