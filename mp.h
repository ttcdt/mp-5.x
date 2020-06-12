/*

    Minimum Profit - A Text Editor

    ttcdt <dev@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#ifdef __cplusplus
extern "C" {
#endif

extern int mp_exit_requested;
#define MP mpdm_get_wcs(mpdm_root(), L"mp")

mpdm_t mp_draw(mpdm_t doc, int optimize);

mpdm_t mp_active(void);
void mp_process_action(mpdm_t action);
void mp_process_event(mpdm_t keycode);
void mp_set_y(mpdm_t doc, int y);
mpdm_t mp_build_status_line(void);
mpdm_t mp_get_history(mpdm_t key);
mpdm_t mp_menu_label(mpdm_t action);
mpdm_t mp_get_doc_names(void);
int mp_keypress_throttle(int keydown);

#ifdef __cplusplus
}
#endif
