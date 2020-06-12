/*

    Minimum Profit - A Text Editor
    GTK driver.

    ttcdt <dev@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include "config.h"

#ifdef CONFOPT_GTK

#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "mpdm.h"
#include "mpsl.h"

#include "mp.h"

#include "mp.xpm"

/** data **/

/* global data */

static GtkWidget *window = NULL;
static GtkWidget *file_tabs = NULL;
static GtkWidget *area = NULL;
static GtkWidget *scrollbar = NULL;
static GtkWidget *status = NULL;
static GtkWidget *menu_bar = NULL;

/* font information */
static int font_width = 0;
static int font_height = 0;
static PangoFontDescription *font = NULL;

/* the attributes */
#define MAX_COLORS 100
static GdkColor inks[MAX_COLORS];
static GdkColor papers[MAX_COLORS];
static int underlines[MAX_COLORS];

#if CONFOPT_GTK == 2
static GdkColor normal_paper;
#endif

#if CONFOPT_GTK == 3
static GdkRGBA normal_paper;
#endif

/* global modal status */
/* code for the 'normal' attribute */
static int normal_attr = 0;

/* mouse down flag */
static int mouse_down = 0;

/* maximize wanted? */
static int maximize = 0;

/* window state variables */
int ls_x, ls_y, ls_w, ls_h;


/** code **/

/** support functions **/

#define L(m) (translate_mbs(m))
#define LL(m) (m)

static char *translate_mbs(char *from)
{
    mpdm_t h, v, w;

    if ((h = mpdm_get_wcs(mpdm_root(), L"__I18N_MBS__")) == NULL)
        h = mpdm_set_wcs(mpdm_root(), MPDM_O(), L"__I18N_MBS__");

    v = mpdm_gettext(MPDM_MBS(from));

    if ((w = mpdm_get(h, v)) == NULL)
        w = mpdm_set(h, MPDM_2MBS(mpdm_string(v)), v);

    return (char *)w->data;
}


static char *wcs_to_utf8(const wchar_t * wptr)
/* converts a wcs to utf-8 */
{
    char *ptr;
    gsize i, o;

    i = wcslen(wptr);

    /* do the conversion */
    ptr = g_convert((const gchar *) wptr, (i + 1) * sizeof(wchar_t),
                    "UTF-8", "WCHAR_T", NULL, &o, NULL);

    return ptr;
}


static char *v_to_utf8(mpdm_t v)
{
    char *ptr = NULL;

    if (v != NULL) {
        mpdm_ref(v);
        ptr = wcs_to_utf8(mpdm_string(v));
        mpdm_unref(v);
    }

    return ptr;
}


static void update_window_size(void)
/* updates the viewport size in characters */
{
    PangoLayout *pa;
    int tx, ty;
    mpdm_t v;

    /* get font metrics */
    pa = gtk_widget_create_pango_layout(area, "m");
    pango_layout_set_font_description(pa, font);
    pango_layout_get_pixel_size(pa, &font_width, &font_height);
    g_object_unref(pa);

    /* calculate the size in chars */
#if CONFOPT_GTK == 2
    tx = (area->allocation.width / font_width);
    ty = (area->allocation.height / font_height) + 1;
#endif

#if CONFOPT_GTK == 3
    tx = (gtk_widget_get_allocated_width(area) / font_width);
    ty = (gtk_widget_get_allocated_height(area) / font_height) + 1;
#endif

    /* store the 'window' size */
    v = mpdm_get_wcs(MP, L"window");
    mpdm_set_wcs(v, MPDM_I(tx), L"tx");
    mpdm_set_wcs(v, MPDM_I(ty), L"ty");

    /* rebuild the pixmap for the double buffer */
}


static void build_fonts(void)
/* builds the fonts */
{
    char tmp[128];
    mpdm_t c;
    mpdm_t w = NULL;
    int font_size           = 12;
    const char *font_face   = "Mono";
    double font_weight      = 0.0;

    if (font != NULL)
        pango_font_description_free(font);

    /* get current configuration */
    if ((c = mpdm_get_wcs(MP, L"config")) != NULL) {
        mpdm_t v;

        if ((v = mpdm_get_wcs(c, L"font_size")) != NULL)
            font_size = mpdm_ival(v);
        else
            mpdm_set_wcs(c, MPDM_I(font_size), L"font_size");

        if ((v = mpdm_get_wcs(c, L"font_weight")) != NULL)
            font_weight = mpdm_rval(v) * 1000.0;
        else
            mpdm_set_wcs(c, MPDM_R(font_weight / 1000.0), L"font_weight");

        if ((v = mpdm_get_wcs(c, L"font_face")) != NULL) {
            w = mpdm_ref(MPDM_2MBS(v->data));
            font_face = w->data;
        }
        else
            mpdm_set_wcs(c, MPDM_MBS(font_face), L"font_face");
    }

    snprintf(tmp, sizeof(tmp) - 1, "%s Thin %d", font_face, font_size);
    tmp[sizeof(tmp) - 1] = '\0';

    font = pango_font_description_from_string(tmp);

    if (font_weight > 0.0)
        pango_font_description_set_weight(font, font_weight);

    update_window_size();

    mpdm_unref(w);
}


static void build_color(GdkColor * gdkcolor, int rgb)
/* builds a color */
{
    gdkcolor->pixel = 0;
    gdkcolor->blue  = (rgb & 0x000000ff) << 8;
    gdkcolor->green = (rgb & 0x0000ff00);
    gdkcolor->red   = (rgb & 0x00ff0000) >> 8;

#if CONFOPT_GTK == 2
    gdk_colormap_alloc_color(gdk_colormap_get_system(), gdkcolor, FALSE, TRUE);
#endif
}


static void build_colors(void)
/* builds the colors */
{
    mpdm_t colors;
    mpdm_t v, i;
    int n, c;

    /* gets the color definitions and attribute names */
    colors = mpdm_get_wcs(MP, L"colors");

    /* loop the colors */
    n = c = 0;
    while (mpdm_iterator(colors, &c, &v, &i)) {
        int ink, paper;

        mpdm_t w = mpdm_get_wcs(v, L"gui");

        /* store the 'normal' attribute */
        if (wcscmp(mpdm_string(i), L"normal") == 0)
            normal_attr = n;

        /* store the attr */
        mpdm_set_wcs(v, MPDM_I(n), L"attr");

        ink   = mpdm_ival(mpdm_get_i(w, 0));
        paper = mpdm_ival(mpdm_get_i(w, 1));

        /* flags */
        w = mpdm_get_wcs(v, L"flags");
        underlines[n] = mpdm_seek_wcs(w, L"underline", 1) != -1 ? 1 : 0;

        if (mpdm_seek_wcs(w, L"reverse", 1) != -1) {
            int t;

            t = ink;
            ink = paper;
            paper = t;
        }

        build_color(&inks[n],   ink);
        build_color(&papers[n], paper);

        n++;
    }

#if CONFOPT_GTK == 2
    normal_paper = papers[normal_attr];
#endif

#if CONFOPT_GTK == 3
    normal_paper.red   = papers[normal_attr].red   / 65535.0;
    normal_paper.green = papers[normal_attr].green / 65535.0;
    normal_paper.blue  = papers[normal_attr].blue  / 65535.0;
    normal_paper.alpha = 1.0;
#endif

}


/** menu functions **/

static void menu_item_callback(mpdm_t action)
/* menu click callback */
{
    mp_process_action(action);
    gtk_widget_queue_draw(GTK_WIDGET(area));

//    gtk_widget_hide(menu_bar);

    if (mp_exit_requested)
        gtk_main_quit();
}


static void build_submenu(GtkWidget * menu, mpdm_t labels)
/* build a submenu */
{
    int n;
    GtkWidget *menu_item;

    mpdm_ref(labels);

    for (n = 0; n < mpdm_size(labels); n++) {
        /* get the action */
        mpdm_t v = mpdm_get_i(labels, n);

        /* if the action is a separator... */
        if (*((wchar_t *) v->data) == L'-')
            menu_item = gtk_separator_menu_item_new();
        else {
            char *ptr;

            ptr = v_to_utf8(mp_menu_label(v));
            menu_item = gtk_menu_item_new_with_label(ptr);
            g_free(ptr);
        }

#if CONFOPT_GTK == 2
        gtk_menu_append(GTK_MENU(menu), menu_item);
#endif
#if CONFOPT_GTK == 3
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
#endif

        g_signal_connect_swapped(G_OBJECT(menu_item), "activate",
                                 G_CALLBACK(menu_item_callback), v);
        gtk_widget_show(menu_item);
    }

    mpdm_unref(labels);
}


static void build_menu(void)
/* builds the menu */
{
    int n;
    mpdm_t m;

    m = mpdm_get_wcs(MP, L"menu");

    /* create a new menu */
    menu_bar = gtk_menu_bar_new();

    for (n = 0; n < mpdm_size(m); n++) {
        char *ptr;
        mpdm_t mi;
        mpdm_t v;
        GtkWidget *menu;
        GtkWidget *menu_item;
        int i;

        /* get the label and the items */
        mi = mpdm_get_i(m, n);
        v = mpdm_get_i(mi, 0);

        ptr = v_to_utf8(mpdm_gettext(v));

        /* change the & by _ for the mnemonic */
        for (i = 0; ptr[i]; i++)
            if (ptr[i] == '&')
                ptr[i] = '_';

        /* add the menu and the label */
        menu = gtk_menu_new();
        menu_item = gtk_menu_item_new_with_mnemonic(ptr);
        g_free(ptr);

        gtk_widget_show(menu_item);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), menu);

#if CONFOPT_GTK == 2
        gtk_menu_bar_append(GTK_MENU_BAR(menu_bar), menu_item);
#endif
#if CONFOPT_GTK == 3
        gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), menu_item);
#endif

        /* now loop the items */
        build_submenu(menu, mpdm_get_i(mi, 1));
    }
}


/** main area drawing functions **/

static void switch_page(GtkNotebook *nb, gpointer *p, gint pg_num, gpointer d)
/* 'switch_page' handler (filetabs) */
{
    /* sets the active one */
    mpdm_set_wcs(MP, MPDM_I(pg_num), L"active_i");

    gtk_widget_grab_focus(area);
    gtk_widget_queue_draw(GTK_WIDGET(area));
}


static void draw_filetabs(void)
/* draws the filetabs */
{
    static mpdm_t prev = NULL;
    mpdm_t names;
    int n;

    names = mpdm_ref(mp_get_doc_names());

    /* disconnect redraw signal to avoid infinite loops */
    g_signal_handlers_disconnect_by_func(G_OBJECT(file_tabs),
                                         G_CALLBACK(switch_page), NULL);

    /* is the list different from the previous one? */
    if (mpdm_cmp(names, prev) != 0) {
        /* delete the current tabs */
        for (n = 0; n < mpdm_size(prev); n++)
            gtk_notebook_remove_page(GTK_NOTEBOOK(file_tabs), 0);

        /* create the new ones */
        for (n = 0; n < mpdm_size(names); n++) {
            GtkWidget *p;
            GtkWidget *f;
            char *ptr;
            mpdm_t v = mpdm_get_i(names, n);

            if ((ptr = v_to_utf8(v)) != NULL) {
                p = gtk_label_new(ptr);
                gtk_widget_show(p);

                f = gtk_frame_new(NULL);
                gtk_widget_show(f);

                gtk_notebook_append_page(GTK_NOTEBOOK(file_tabs), f, p);

                g_free(ptr);
            }
        }

        /* store for the next time */
        mpdm_store(&prev, names);
    }

    mpdm_unref(names);

    /* set the active one */
    gtk_notebook_set_current_page(GTK_NOTEBOOK(file_tabs),
                          mpdm_ival(mpdm_get_wcs(MP, L"active_i")));

    /* reconnect signal */
    g_signal_connect(G_OBJECT(file_tabs), "switch_page",
                     G_CALLBACK(switch_page), NULL);

    gtk_widget_grab_focus(area);
}


static void draw_status(void)
/* draws the status line */
{
    char *ptr;

    ptr = v_to_utf8(mp_build_status_line());
    gtk_label_set_text(GTK_LABEL(status), ptr);
    g_free(ptr);
}


static gint scroll_event(GtkWidget *widget, GdkEventScroll *event)
/* 'scroll_event' handler (mouse wheel) */
{
    double dx, dy;
    wchar_t *ptr = NULL;

    switch (event->direction) {
    case GDK_SCROLL_UP:
        ptr = L"mouse-wheel-up";
        break;
    case GDK_SCROLL_DOWN:
        ptr = L"mouse-wheel-down";
        break;
    case GDK_SCROLL_LEFT:
        ptr = L"mouse-wheel-left";
        break;
    case GDK_SCROLL_RIGHT:
        ptr = L"mouse-wheel-right";
        break;

#if CONFOPT_GTK == 3
    case GDK_SCROLL_SMOOTH:
        gdk_event_get_scroll_deltas((GdkEvent *)event, &dx, &dy);

        if (dy > 0)
            ptr = L"mouse-wheel-down";
        else
        if (dy < 0)
            ptr = L"mouse-wheel-up";

        break;
#endif

    default:
        ptr = NULL;
        break;
    }

    if (ptr != NULL) {
        mp_process_event(MPDM_S(ptr));
        gtk_widget_queue_draw(GTK_WIDGET(area));
    }

    return 0;
}


static void value_changed(GtkAdjustment * adj, gpointer * data)
/* 'value_changed' handler (scrollbar) */
{
    int i = (int) gtk_adjustment_get_value(adj);
    mpdm_t doc;
    mpdm_t txt;
    int y;

    /* get current y position */
    doc = mp_active();
    txt = mpdm_get_wcs(doc, L"txt");
    y = mpdm_ival(mpdm_get_wcs(txt, L"y"));

    /* if it's different, set and redraw */
    if (y != i) {
        mp_set_y(doc, i);
        mpdm_set_wcs(txt, MPDM_I(i), L"vy");
        gtk_widget_queue_draw(GTK_WIDGET(area));
    }
}


static void draw_scrollbar(void)
/* updates the scrollbar */
{
    GtkAdjustment *adjustment;
    mpdm_t d;
    mpdm_t v;
    int pos, size, max;

    d = mp_active();

    /* get the coordinates */
    v = mpdm_get_wcs(d, L"txt");
    pos = mpdm_ival(mpdm_get_wcs(v, L"vy"));
    max = mpdm_size(mpdm_get_wcs(v, L"lines"));

    v = mpdm_get_wcs(MP, L"window");
    size = mpdm_ival(mpdm_get_wcs(v, L"ty"));

    adjustment = gtk_range_get_adjustment(GTK_RANGE(scrollbar));

    /* disconnect to avoid infinite loops */
    g_signal_handlers_disconnect_by_func(G_OBJECT(adjustment),
                                         G_CALLBACK(value_changed), NULL);

    gtk_adjustment_set_step_increment(adjustment, (gdouble) 1);
    gtk_adjustment_set_upper(adjustment, (gdouble) max);
    gtk_adjustment_set_page_size(adjustment, (gdouble) size);
    gtk_adjustment_set_page_increment(adjustment, (gdouble) size);
    gtk_adjustment_set_value(adjustment, (gdouble) pos);

    gtk_range_set_adjustment(GTK_RANGE(scrollbar), adjustment);

    /* reattach again */
    g_signal_connect(G_OBJECT(gtk_range_get_adjustment(GTK_RANGE(scrollbar))),
                     "value_changed", G_CALLBACK(value_changed), NULL);
}


static void gtk_drv_render(mpdm_t doc, int optimize)
/* GTK document draw function */
{
    GdkRectangle gr;
    cairo_t *cr;
    mpdm_t d = NULL;
    int n, m;

    if (maximize)
        gtk_window_maximize(GTK_WINDOW(window));

    /* no gc? create it */
    if (font == NULL)
        build_fonts();

    d = mp_draw(doc, optimize);

    mpdm_ref(d);

    gr.x = 0;
    gr.y = 0;

#if CONFOPT_GTK == 2
    gr.width = area->allocation.width;
#endif
#if CONFOPT_GTK == 3
    gr.width = gtk_widget_get_allocated_width(area);
#endif

    gr.height = font_height;

#if GDK_MAJOR_VERSION > 3 || GDK_MINOR_VERSION >= 22
    GdkDrawingContext *gdc;
    gdc = gdk_window_begin_draw_frame(gtk_widget_get_window(area),
        cairo_region_create());
    cr = gdk_drawing_context_get_cairo_context(gdc);
#else
    cr = gdk_cairo_create(gtk_widget_get_window(area));
#endif

    for (n = 0; n < mpdm_size(d); n++) {
        PangoLayout *pl;
        PangoAttrList *pal;
        mpdm_t l = mpdm_get_i(d, n);
        char *str = NULL;
        int u;
        int p = 0;

        if (l == NULL)
            continue;

        /* create the pango stuff */
        pl = gtk_widget_create_pango_layout(area, NULL);
        pango_layout_set_font_description(pl, font);
        pal = pango_attr_list_new();

        for (m = u = p = 0; m < mpdm_size(l); m++, u = p) {
            PangoAttribute *pa;
            int attr;
            mpdm_t s;
            char *ptr;

            /* get the attribute and the string */
            attr = mpdm_ival(mpdm_get_i(l, m++));
            s = mpdm_get_i(l, m);

            /* convert the string to utf8 */
            ptr = v_to_utf8(s);

            /* add to the full line */
            str = mpdm_poke(str, &p, ptr, strlen(ptr), 1);

            g_free(ptr);

            /* create the background if it's
               different from the default */
            if (papers[attr].red   != papers[normal_attr].red ||
                papers[attr].green != papers[normal_attr].green ||
                papers[attr].blue  != papers[normal_attr].blue) {
                pa = pango_attr_background_new(papers[attr].red,
                                               papers[attr].green,
                                               papers[attr].blue);

                pa->start_index = u;
                pa->end_index   = p;

                pango_attr_list_insert(pal, pa);
            }

            /* underline? */
            if (underlines[attr]) {
                pa = pango_attr_underline_new(TRUE);

                pa->start_index = u;
                pa->end_index   = p;

                pango_attr_list_insert(pal, pa);
            }

            /* foreground color */
            pa = pango_attr_foreground_new(inks[attr].red,
                                           inks[attr].green,
                                           inks[attr].blue);

            pa->start_index = u;
            pa->end_index   = p;

            pango_attr_list_insert(pal, pa);
        }

        /* store the attributes */
        pango_layout_set_attributes(pl, pal);
        pango_attr_list_unref(pal);

        /* store and free the text */
        pango_layout_set_text(pl, str, p);
        free(str);

        /* draw the background */
#if CONFOPT_GTK == 2
        gdk_cairo_set_source_color(cr, &normal_paper);
#endif

#if CONFOPT_GTK == 3
        gdk_cairo_set_source_rgba(cr, &normal_paper);
#endif

        cairo_rectangle(cr, 0, n * font_height, gr.width, gr.height);
        cairo_fill(cr);

        /* draw the text */
        cairo_move_to(cr, 2, n * font_height);
        pango_cairo_show_layout(cr, pl);

        g_object_unref(pl);
    }

#if GDK_MAJOR_VERSION > 3 || GDK_MINOR_VERSION >= 22
    gdk_window_end_draw_frame(gtk_widget_get_window(area), gdc);
#else
    cairo_destroy(cr);
#endif

    mpdm_unref(d);

    draw_filetabs();
    draw_scrollbar();
    draw_status();

    gtk_window_get_position(GTK_WINDOW(window), &ls_x, &ls_y);
}


static gint delete_event(GtkWidget * w, GdkEvent * e, gpointer data)
/* 'delete_event' handler */
{
    mp_process_event(MPDM_S(L"close-window"));

    return mp_exit_requested ? FALSE : TRUE;
}


static void destroy(GtkWidget * w, gpointer data)
/* 'destroy' handler */
{
    gtk_main_quit();
}


#if 0
static gint key_release_event(GtkWidget *w, GdkEventKey *e, gpointer d)
/* 'key_release_event' handler */
{
    if (mp_keypress_throttle(0))
        gtk_widget_queue_draw(GTK_WIDGET(area));

    return 0;
}
#endif


#if CONFOPT_GTK == 2
#define MP_KEY_Up GDK_Up
#define MP_KEY_Down GDK_Down
#define MP_KEY_Left GDK_Left
#define MP_KEY_Right GDK_Right
#define MP_KEY_Prior GDK_Prior
#define MP_KEY_Next GDK_Next
#define MP_KEY_Home GDK_Home
#define MP_KEY_End GDK_End
#define MP_KEY_space GDK_space
#define MP_KEY_KP_Add GDK_KP_Add
#define MP_KEY_KP_Subtract GDK_KP_Subtract
#define MP_KEY_KP_Multiply GDK_KP_Multiply
#define MP_KEY_KP_Divide GDK_KP_Divide
#define MP_KEY_F1 GDK_F1
#define MP_KEY_F2 GDK_F2
#define MP_KEY_F3 GDK_F3
#define MP_KEY_F4 GDK_F4
#define MP_KEY_F5 GDK_F5
#define MP_KEY_F6 GDK_F6
#define MP_KEY_F7 GDK_F7
#define MP_KEY_F8 GDK_F8
#define MP_KEY_F9 GDK_F9
#define MP_KEY_F10 GDK_F10
#define MP_KEY_F11 GDK_F11
#define MP_KEY_F12 GDK_F12
#define MP_KEY_KP_Enter GDK_KP_Enter
#define MP_KEY_Return GDK_Return
#define MP_KEY_Cyrillic_ve GDK_Cyrillic_ve
#define MP_KEY_Cyrillic_a GDK_Cyrillic_a
#define MP_KEY_Cyrillic_tse GDK_Cyrillic_tse
#define MP_KEY_Cyrillic_de GDK_Cyrillic_de
#define MP_KEY_Cyrillic_ie GDK_Cyrillic_ie
#define MP_KEY_Cyrillic_ef GDK_Cyrillic_ef
#define MP_KEY_Cyrillic_ghe GDK_Cyrillic_ghe
#define MP_KEY_Cyrillic_i GDK_Cyrillic_i
#define MP_KEY_Cyrillic_shorti GDK_Cyrillic_shorti
#define MP_KEY_Cyrillic_ka GDK_Cyrillic_ka
#define MP_KEY_Cyrillic_el GDK_Cyrillic_el
#define MP_KEY_Cyrillic_em GDK_Cyrillic_em
#define MP_KEY_Cyrillic_en GDK_Cyrillic_en
#define MP_KEY_Cyrillic_o GDK_Cyrillic_o
#define MP_KEY_Cyrillic_pe GDK_Cyrillic_pe
#define MP_KEY_Cyrillic_ya GDK_Cyrillic_ya
#define MP_KEY_Cyrillic_er GDK_Cyrillic_er
#define MP_KEY_Cyrillic_es GDK_Cyrillic_es
#define MP_KEY_Cyrillic_te GDK_Cyrillic_te
#define MP_KEY_Cyrillic_softsign GDK_Cyrillic_softsign
#define MP_KEY_Cyrillic_yeru GDK_Cyrillic_yeru
#define MP_KEY_Cyrillic_ze GDK_Cyrillic_ze
#define MP_KEY_Cyrillic_sha GDK_Cyrillic_sha
#define MP_KEY_Cyrillic_e GDK_Cyrillic_e
#define MP_KEY_Cyrillic_shcha GDK_Cyrillic_shcha
#define MP_KEY_Cyrillic_che GDK_Cyrillic_che
#define MP_KEY_Tab GDK_Tab
#define MP_KEY_ISO_Left_Tab GDK_ISO_Left_Tab
#define MP_KEY_Escape GDK_Escape
#define MP_KEY_Insert GDK_Insert
#define MP_KEY_BackSpace GDK_BackSpace
#define MP_KEY_Delete GDK_Delete
#endif /* CONFOPT_GTK == 2 */

#if CONFOPT_GTK == 3
#define MP_KEY_Up GDK_KEY_Up
#define MP_KEY_Down GDK_KEY_Down
#define MP_KEY_Left GDK_KEY_Left
#define MP_KEY_Right GDK_KEY_Right
#define MP_KEY_Prior GDK_KEY_Prior
#define MP_KEY_Next GDK_KEY_Next
#define MP_KEY_Home GDK_KEY_Home
#define MP_KEY_End GDK_KEY_End
#define MP_KEY_space GDK_KEY_space
#define MP_KEY_KP_Add GDK_KEY_KP_Add
#define MP_KEY_KP_Subtract GDK_KEY_KP_Subtract
#define MP_KEY_KP_Multiply GDK_KEY_KP_Multiply
#define MP_KEY_KP_Divide GDK_KEY_KP_Divide
#define MP_KEY_F1 GDK_KEY_F1
#define MP_KEY_F2 GDK_KEY_F2
#define MP_KEY_F3 GDK_KEY_F3
#define MP_KEY_F4 GDK_KEY_F4
#define MP_KEY_F5 GDK_KEY_F5
#define MP_KEY_F6 GDK_KEY_F6
#define MP_KEY_F7 GDK_KEY_F7
#define MP_KEY_F8 GDK_KEY_F8
#define MP_KEY_F9 GDK_KEY_F9
#define MP_KEY_F10 GDK_KEY_F10
#define MP_KEY_F11 GDK_KEY_F11
#define MP_KEY_F12 GDK_KEY_F12
#define MP_KEY_KP_Enter GDK_KEY_KP_Enter
#define MP_KEY_Return GDK_KEY_Return
#define MP_KEY_Cyrillic_ve GDK_KEY_Cyrillic_ve
#define MP_KEY_Cyrillic_a GDK_KEY_Cyrillic_a
#define MP_KEY_Cyrillic_tse GDK_KEY_Cyrillic_tse
#define MP_KEY_Cyrillic_de GDK_KEY_Cyrillic_de
#define MP_KEY_Cyrillic_ie GDK_KEY_Cyrillic_ie
#define MP_KEY_Cyrillic_ef GDK_KEY_Cyrillic_ef
#define MP_KEY_Cyrillic_ghe GDK_KEY_Cyrillic_ghe
#define MP_KEY_Cyrillic_i GDK_KEY_Cyrillic_i
#define MP_KEY_Cyrillic_shorti GDK_KEY_Cyrillic_shorti
#define MP_KEY_Cyrillic_ka GDK_KEY_Cyrillic_ka
#define MP_KEY_Cyrillic_el GDK_KEY_Cyrillic_el
#define MP_KEY_Cyrillic_em GDK_KEY_Cyrillic_em
#define MP_KEY_Cyrillic_en GDK_KEY_Cyrillic_en
#define MP_KEY_Cyrillic_o GDK_KEY_Cyrillic_o
#define MP_KEY_Cyrillic_pe GDK_KEY_Cyrillic_pe
#define MP_KEY_Cyrillic_ya GDK_KEY_Cyrillic_ya
#define MP_KEY_Cyrillic_er GDK_KEY_Cyrillic_er
#define MP_KEY_Cyrillic_es GDK_KEY_Cyrillic_es
#define MP_KEY_Cyrillic_te GDK_KEY_Cyrillic_te
#define MP_KEY_Cyrillic_softsign GDK_KEY_Cyrillic_softsign
#define MP_KEY_Cyrillic_yeru GDK_KEY_Cyrillic_yeru
#define MP_KEY_Cyrillic_ze GDK_KEY_Cyrillic_ze
#define MP_KEY_Cyrillic_sha GDK_KEY_Cyrillic_sha
#define MP_KEY_Cyrillic_e GDK_KEY_Cyrillic_e
#define MP_KEY_Cyrillic_shcha GDK_KEY_Cyrillic_shcha
#define MP_KEY_Cyrillic_che GDK_KEY_Cyrillic_che
#define MP_KEY_Tab GDK_KEY_Tab
#define MP_KEY_ISO_Left_Tab GDK_KEY_ISO_Left_Tab
#define MP_KEY_Escape GDK_KEY_Escape
#define MP_KEY_Insert GDK_KEY_Insert
#define MP_KEY_BackSpace GDK_KEY_BackSpace
#define MP_KEY_Delete GDK_KEY_Delete
#endif /* CONFOPT_GTK == 3 */

static void im_commit(GtkIMContext *i, char *str, gpointer u)
/* GtkIM 'commit' handler */
{
    wchar_t *im_char = (wchar_t *)u;
    int s = 0;

    /* convert the utf-8 string to a wchar_t */
    while (*str)
        mpdm_utf8_to_wc(im_char, &s, *str++);

    im_char[1] = L'\0';
}


static gint key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer data)
/* 'key_press_event' handler */
{
    static GtkIMContext *im = NULL;
    static wchar_t im_char[2] = L"";
    wchar_t *ptr = NULL;

    if (im == NULL) {
        im = gtk_im_multicontext_new();
        g_signal_connect(im, "commit", G_CALLBACK(im_commit), im_char);
        gtk_im_context_set_client_window(im, gtk_widget_get_window(widget));
    }

    gtk_im_context_filter_keypress(im, event);

    /* set mp.shift_pressed */
    if (event->state & (GDK_SHIFT_MASK))
        mpdm_set_wcs(MP, MPDM_I(1), L"shift_pressed");

    if (event->state & (GDK_SHIFT_MASK)) {
        switch (event->keyval) {
        case MP_KEY_F1:
            ptr = L"shift-f1";
            break;
        case MP_KEY_F2:
            ptr = L"shift-f2";
            break;
        case MP_KEY_F3:
            ptr = L"shift-f3";
            break;
        case MP_KEY_F4:
            ptr = L"shift-f4";
            break;
        case MP_KEY_F5:
            ptr = L"shift-f5";
            break;
        case MP_KEY_F6:
            ptr = L"shift-f6";
            break;
        case MP_KEY_F7:
            ptr = L"shift-f7";
            break;
        case MP_KEY_F8:
            ptr = L"shift-f8";
            break;
        case MP_KEY_F9:
            ptr = L"shift-f9";
            break;
        case MP_KEY_F10:
            ptr = L"shift-f10";
            break;
        case MP_KEY_F11:
            ptr = L"shift-f11";
            break;
        case MP_KEY_F12:
            ptr = L"shift-f12";
            break;
        }
    }

    /* reserve alt for menu mnemonics */
/*  if (GDK_MOD1_MASK & event->state)
        return(0);*/

    if (ptr == NULL && (event->state & (GDK_CONTROL_MASK))) {
        switch (event->keyval) {
        case MP_KEY_Up:
            ptr = L"ctrl-cursor-up";
            break;
        case MP_KEY_Down:
            ptr = L"ctrl-cursor-down";
            break;
        case MP_KEY_Left:
            ptr = L"ctrl-cursor-left";
            break;
        case MP_KEY_Right:
            ptr = L"ctrl-cursor-right";
            break;
        case MP_KEY_Prior:
            ptr = L"ctrl-page-up";
            break;
        case MP_KEY_Next:
            ptr = L"ctrl-page-down";
            break;
        case MP_KEY_Home:
            ptr = L"ctrl-home";
            break;
        case MP_KEY_End:
            ptr = L"ctrl-end";
            break;
        case MP_KEY_space:
            ptr = L"ctrl-space";
            break;
        case MP_KEY_KP_Add:
            ptr = L"ctrl-kp-plus";
            break;
        case MP_KEY_KP_Subtract:
            ptr = L"ctrl-kp-minus";
            break;
        case MP_KEY_KP_Multiply:
            ptr = L"ctrl-kp-multiply";
            break;
        case MP_KEY_KP_Divide:
            ptr = L"ctrl-kp-divide";
            break;
        case MP_KEY_F1:
            ptr = L"ctrl-f1";
            break;
        case MP_KEY_F2:
            ptr = L"ctrl-f2";
            break;
        case MP_KEY_F3:
            ptr = L"ctrl-f3";
            break;
        case MP_KEY_F4:
            ptr = L"ctrl-f4";
            break;
        case MP_KEY_F5:
            ptr = L"ctrl-f5";
            break;
        case MP_KEY_F6:
            ptr = L"ctrl-f6";
            break;
        case MP_KEY_F7:
            ptr = L"ctrl-f7";
            break;
        case MP_KEY_F8:
            ptr = L"ctrl-f8";
            break;
        case MP_KEY_F9:
            ptr = L"ctrl-f9";
            break;
        case MP_KEY_F10:
            ptr = L"ctrl-f10";
            break;
        case MP_KEY_F11:
            ptr = L"ctrl-f11";
            break;
        case MP_KEY_F12:
            ptr = L"ctrl-f12";
            break;
        case MP_KEY_KP_Enter:
        case MP_KEY_Return:
            ptr = L"ctrl-enter";
            break;
        case MP_KEY_Cyrillic_ve:
            ptr = L"ctrl-d";
            break;
        case MP_KEY_Cyrillic_a:
            ptr = L"ctrl-f";
            break;
        case MP_KEY_Cyrillic_tse:
            ptr = L"ctrl-w";
            break;
        case MP_KEY_Cyrillic_de:
            ptr = L"ctrl-l";
            break;
        case MP_KEY_Cyrillic_ie:
            ptr = L"ctrl-t";
            break;
        case MP_KEY_Cyrillic_ef:
            ptr = L"ctrl-a";
            break;
        case MP_KEY_Cyrillic_ghe:
            ptr = L"ctrl-u";
            break;
        case MP_KEY_Cyrillic_i:
            ptr = L"ctrl-b";
            break;
        case MP_KEY_Cyrillic_shorti:
            ptr = L"ctrl-q";
            break;
        case MP_KEY_Cyrillic_ka:
            ptr = L"ctrl-r";
            break;
        case MP_KEY_Cyrillic_el:
            ptr = L"ctrl-k";
            break;
        case MP_KEY_Cyrillic_em:
            ptr = L"ctrl-v";
            break;
        case MP_KEY_Cyrillic_en:
            ptr = L"ctrl-y";
            break;
        case MP_KEY_Cyrillic_o:
            ptr = L"ctrl-j";
            break;
        case MP_KEY_Cyrillic_pe:
            ptr = L"ctrl-g";
            break;
        case MP_KEY_Cyrillic_ya:
            ptr = L"ctrl-z";
            break;
        case MP_KEY_Cyrillic_er:
            ptr = L"ctrl-h";
            break;
        case MP_KEY_Cyrillic_es:
            ptr = L"ctrl-c";
            break;
        case MP_KEY_Cyrillic_te:
            ptr = L"ctrl-n";
            break;
        case MP_KEY_Cyrillic_softsign:
            ptr = L"ctrl-m";
            break;
        case MP_KEY_Cyrillic_yeru:
            ptr = L"ctrl-s";
            break;
        case MP_KEY_Cyrillic_ze:
            ptr = L"ctrl-p";
            break;
        case MP_KEY_Cyrillic_sha:
            ptr = L"ctrl-i";
            break;
        case MP_KEY_Cyrillic_e:
            ptr = L"ctrl-t";
            break;
        case MP_KEY_Cyrillic_shcha:
            ptr = L"ctrl-o";
            break;
        case MP_KEY_Cyrillic_che:
            ptr = L"ctrl-x";
            break;
        }

        if (ptr == NULL) {
            char c = event->keyval & 0xdf;

            switch (c) {
            case 'A':
                ptr = L"ctrl-a";
                break;
            case 'B':
                ptr = L"ctrl-b";
                break;
            case 'C':
                ptr = L"ctrl-c";
                break;
            case 'D':
                ptr = L"ctrl-d";
                break;
            case 'E':
                ptr = L"ctrl-e";
                break;
            case 'F':
                ptr = L"ctrl-f";
                break;
            case 'G':
                ptr = L"ctrl-g";
                break;
            case 'H':
                ptr = L"ctrl-h";
                break;
            case 'I':
                ptr = L"ctrl-i";
                break;
            case 'J':
                ptr = L"ctrl-j";
                break;
            case 'K':
                ptr = L"ctrl-k";
                break;
            case 'L':
                ptr = L"ctrl-l";
                break;
            case 'M':
                ptr = L"ctrl-m";
                break;
            case 'N':
                ptr = L"ctrl-n";
                break;
            case 'O':
                ptr = L"ctrl-o";
                break;
            case 'P':
                ptr = L"ctrl-p";
                break;
            case 'Q':
                ptr = L"ctrl-q";
                break;
            case 'R':
                ptr = L"ctrl-r";
                break;
            case 'S':
                ptr = L"ctrl-s";
                break;
            case 'T':
                ptr = L"ctrl-t";
                break;
            case 'U':
                ptr = L"ctrl-u";
                break;
            case 'V':
                ptr = L"ctrl-v";
                break;
            case 'W':
                ptr = L"ctrl-w";
                break;
            case 'X':
                ptr = L"ctrl-x";
                break;
            case 'Y':
                ptr = L"ctrl-y";
                break;
            case 'Z':
                ptr = L"ctrl-z";
                break;
            }
        }
    }
    else
    if (ptr == NULL && (event->state & (GDK_MOD1_MASK))) {
        switch (event->keyval) {
        case MP_KEY_Up:
            ptr = L"alt-cursor-up";
            break;
        case MP_KEY_Down:
            ptr = L"alt-cursor-down";
            break;
        case MP_KEY_Left:
            ptr = L"alt-cursor-left";
            break;
        case MP_KEY_Right:
            ptr = L"alt-cursor-right";
            break;
        case MP_KEY_Prior:
            ptr = L"alt-page-up";
            break;
        case MP_KEY_Next:
            ptr = L"alt-page-down";
            break;
        case MP_KEY_Home:
            ptr = L"alt-home";
            break;
        case MP_KEY_End:
            ptr = L"alt-end";
            break;
        case MP_KEY_space:
            ptr = L"alt-space";
            break;
        case MP_KEY_KP_Add:
            ptr = L"alt-kp-plus";
            break;
        case MP_KEY_KP_Subtract:
            ptr = L"alt-kp-minus";
            break;
        case MP_KEY_KP_Multiply:
            ptr = L"alt-kp-multiply";
            break;
        case MP_KEY_KP_Divide:
            ptr = L"alt-kp-divide";
            break;
        case MP_KEY_F1:
            ptr = L"alt-f1";
            break;
        case MP_KEY_F2:
            ptr = L"alt-f2";
            break;
        case MP_KEY_F3:
            ptr = L"alt-f3";
            break;
        case MP_KEY_F4:
            ptr = L"alt-f4";
            break;
        case MP_KEY_F5:
            ptr = L"alt-f5";
            break;
        case MP_KEY_F6:
            ptr = L"alt-f6";
            break;
        case MP_KEY_F7:
            ptr = L"alt-f7";
            break;
        case MP_KEY_F8:
            ptr = L"alt-f8";
            break;
        case MP_KEY_F9:
            ptr = L"alt-f9";
            break;
        case MP_KEY_F10:
            ptr = L"alt-f10";
            break;
        case MP_KEY_F11:
            ptr = L"alt-f11";
            break;
        case MP_KEY_F12:
            ptr = L"alt-f12";
            break;
        case MP_KEY_KP_Enter:
        case MP_KEY_Return:
            ptr = L"alt-enter";
            break;
        case MP_KEY_Cyrillic_ve:
            ptr = L"alt-d";
            break;
        case MP_KEY_Cyrillic_a:
            ptr = L"alt-f";
            break;
        case MP_KEY_Cyrillic_tse:
            ptr = L"alt-w";
            break;
        case MP_KEY_Cyrillic_de:
            ptr = L"alt-l";
            break;
        case MP_KEY_Cyrillic_ie:
            ptr = L"alt-t";
            break;
        case MP_KEY_Cyrillic_ef:
            ptr = L"alt-a";
            break;
        case MP_KEY_Cyrillic_ghe:
            ptr = L"alt-u";
            break;
        case MP_KEY_Cyrillic_i:
            ptr = L"alt-b";
            break;
        case MP_KEY_Cyrillic_shorti:
            ptr = L"alt-q";
            break;
        case MP_KEY_Cyrillic_ka:
            ptr = L"alt-r";
            break;
        case MP_KEY_Cyrillic_el:
            ptr = L"alt-k";
            break;
        case MP_KEY_Cyrillic_em:
            ptr = L"alt-v";
            break;
        case MP_KEY_Cyrillic_en:
            ptr = L"alt-y";
            break;
        case MP_KEY_Cyrillic_o:
            ptr = L"alt-j";
            break;
        case MP_KEY_Cyrillic_pe:
            ptr = L"alt-g";
            break;
        case MP_KEY_Cyrillic_ya:
            ptr = L"alt-z";
            break;
        case MP_KEY_Cyrillic_er:
            ptr = L"alt-h";
            break;
        case MP_KEY_Cyrillic_es:
            ptr = L"alt-c";
            break;
        case MP_KEY_Cyrillic_te:
            ptr = L"alt-n";
            break;
        case MP_KEY_Cyrillic_softsign:
            ptr = L"alt-m";
            break;
        case MP_KEY_Cyrillic_yeru:
            ptr = L"alt-s";
            break;
        case MP_KEY_Cyrillic_ze:
            ptr = L"alt-p";
            break;
        case MP_KEY_Cyrillic_sha:
            ptr = L"alt-i";
            break;
        case MP_KEY_Cyrillic_e:
            ptr = L"alt-t";
            break;
        case MP_KEY_Cyrillic_shcha:
            ptr = L"alt-o";
            break;
        case MP_KEY_Cyrillic_che:
            ptr = L"alt-x";
            break;
        }

        if (ptr == NULL) {
            char c = event->keyval & 0xdf;

            switch (c) {
            case 'A':
                ptr = L"alt-a";
                break;
            case 'B':
                ptr = L"alt-b";
                break;
            case 'C':
                ptr = L"alt-c";
                break;
            case 'D':
                ptr = L"alt-d";
                break;
            case 'E':
                ptr = L"alt-e";
                break;
            case 'F':
                ptr = L"alt-f";
                break;
            case 'G':
                ptr = L"alt-g";
                break;
            case 'H':
                ptr = L"alt-h";
                break;
            case 'I':
                ptr = L"alt-i";
                break;
            case 'J':
                ptr = L"alt-j";
                break;
            case 'K':
                ptr = L"alt-k";
                break;
            case 'L':
                ptr = L"alt-l";
                break;
            case 'M':
                ptr = L"alt-m";
                break;
            case 'N':
                ptr = L"alt-n";
                break;
            case 'O':
                ptr = L"alt-o";
                break;
            case 'P':
                ptr = L"alt-p";
                break;
            case 'Q':
                ptr = L"alt-q";
                break;
            case 'R':
                ptr = L"alt-r";
                break;
            case 'S':
                ptr = L"alt-s";
                break;
            case 'T':
                ptr = L"alt-t";
                break;
            case 'U':
                ptr = L"alt-u";
                break;
            case 'V':
                ptr = L"alt-v";
                break;
            case 'W':
                ptr = L"alt-w";
                break;
            case 'X':
                ptr = L"alt-x";
                break;
            case 'Y':
                ptr = L"alt-y";
                break;
            case 'Z':
                ptr = L"alt-z";
                break;
            case '-':
                ptr = L"alt-minus";
                break;
            }
        }
    }
    else 
    if (ptr == NULL) {
        switch (event->keyval) {
        case MP_KEY_Up:
        case GDK_KEY_KP_Up:
            ptr = L"cursor-up";
            break;
        case MP_KEY_Down:
        case GDK_KEY_KP_Down:
            ptr = L"cursor-down";
            break;
        case MP_KEY_Left:
        case GDK_KEY_KP_Left:
            ptr = L"cursor-left";
            break;
        case MP_KEY_Right:
        case GDK_KEY_KP_Right:
            ptr = L"cursor-right";
            break;
        case MP_KEY_Prior:
        case GDK_KEY_KP_Prior:
            ptr = L"page-up";
            break;
        case MP_KEY_Next:
        case GDK_KEY_KP_Next:
            ptr = L"page-down";
            break;
        case MP_KEY_Home:
        case GDK_KEY_KP_Home:
            ptr = L"home";
            break;
        case MP_KEY_End:
        case GDK_KEY_KP_End:
            ptr = L"end";
            break;
        case MP_KEY_space:
            ptr = L"space";
            break;
        case MP_KEY_KP_Add:
            ptr = L"kp-plus";
            break;
        case MP_KEY_KP_Subtract:
            ptr = L"kp-minus";
            break;
        case MP_KEY_KP_Multiply:
            ptr = L"kp-multiply";
            break;
        case MP_KEY_KP_Divide:
            ptr = L"kp-divide";
            break;
        case MP_KEY_F1:
            ptr = L"f1";
            break;
        case MP_KEY_F2:
            ptr = L"f2";
            break;
        case MP_KEY_F3:
            ptr = L"f3";
            break;
        case MP_KEY_F4:
            ptr = L"f4";
            break;
        case MP_KEY_F5:
            ptr = L"f5";
            break;
        case MP_KEY_F6:
            ptr = L"f6";
            break;
        case MP_KEY_F7:
            ptr = L"f7";
            break;
        case MP_KEY_F8:
            ptr = L"f8";
            break;
        case MP_KEY_F9:
            ptr = L"f9";
            break;
        case MP_KEY_F10:
            ptr = L"f10";
            break;
        case MP_KEY_F11:
            ptr = L"f11";
            break;
        case MP_KEY_F12:
            ptr = L"f12";
            break;
        case MP_KEY_Insert:
            ptr = L"insert";
            break;
        case MP_KEY_BackSpace:
            ptr = L"backspace";
            break;
        case MP_KEY_Delete:
            ptr = L"delete";
            break;
        case MP_KEY_KP_Enter:
        case MP_KEY_Return:
            ptr = L"enter";
            break;
        case MP_KEY_Tab:
            ptr = L"tab";
            break;
        case MP_KEY_ISO_Left_Tab:
            ptr = L"shift-tab";
            break;
        case MP_KEY_Escape:
            ptr = L"escape";
            break;
        }
    }

    /* if there is a pending char in im_char, use it */
    if (ptr == NULL && im_char[0] != L'\0')
        ptr = im_char;

//    gtk_widget_hide(menu_bar);

    /* finally process */
    if (ptr != NULL)
        mp_process_event(MPDM_S(ptr));

    /* delete the pending char */
    im_char[0] = L'\0';

    if (mp_exit_requested)
        gtk_main_quit();

    if (ptr)
        gtk_widget_queue_draw(GTK_WIDGET(area));

    return 0;
}


static gint button_press_event(GtkWidget *w, GdkEventButton *event, gpointer d)
/* 'button_press_event' handler (mouse buttons) */
{
    int x, y;
    wchar_t *ptr = NULL;

    mouse_down = 1;

    /* mouse instant positioning */
    x = ((int) event->x) / font_width;
    y = ((int) event->y) / font_height;

    mpdm_set_wcs(MP, MPDM_I(x), L"mouse_x");
    mpdm_set_wcs(MP, MPDM_I(y), L"mouse_y");

    switch (event->button) {
    case 1:
        if (event->type == GDK_2BUTTON_PRESS)
            ptr = L"mouse-left-dblclick";
        else
            ptr = L"mouse-left-button";
        break;
    case 2:
        ptr = L"mouse-middle-button";
        break;
    case 3:
        ptr = L"mouse-right-button";
        break;
    case 4:
        ptr = L"mouse-wheel-up";
        break;
    case 5:
        ptr = L"mouse-wheel-down";
        break;
    }

    if (ptr != NULL)
        mp_process_event(MPDM_S(ptr));

    gtk_widget_queue_draw(GTK_WIDGET(area));

//    gtk_widget_hide(menu_bar);

    return 0;
}


static gint button_release_event(GtkWidget *w, GdkEventButton *e, gpointer d)
/* 'button_release_event' handle (mouse buttons) */
{
    mouse_down = 0;

    return TRUE;
}


static gint motion_notify_event(GtkWidget *w, GdkEventMotion *event, gpointer d)
/* 'motion_notify_event' handler (mouse movement) */
{
    if (mouse_down) {
        int x, y;

        /* mouse dragging */
        x = ((int) event->x) / font_width;
        y = ((int) event->y) / font_height;

        mpdm_set_wcs(MP, MPDM_I(x), L"mouse_to_x");
        mpdm_set_wcs(MP, MPDM_I(y), L"mouse_to_y");

        mp_process_event(MPDM_S(L"mouse-drag"));

        gtk_widget_queue_draw(GTK_WIDGET(area));
    }

    return TRUE;
}


static void drag_data_received(GtkWidget *w, GdkDragContext *dc,
                               gint x, gint y, GtkSelectionData *data,
                               guint info, guint time)
/* 'drag_data_received' handler */
{
    mpdm_t v;

    /* get data */
    v = MPDM_MBS((char *)gtk_selection_data_get_text(data));

    /* strip URI crap */
    v = mpdm_sregex(v, MPDM_S(L"!file://!g"), NULL, 0);

    /* split */
    v = mpdm_split_wcs(v, L"\n");

    /* drop last element, as it's an empty string */
    mpdm_del_i(v, -1);

    mpdm_set_wcs(MP, v, L"dropped_files");

    mp_process_event(MPDM_S(L"dropped-files"));

    gtk_widget_queue_draw(GTK_WIDGET(area));

    gtk_drag_finish(dc, TRUE, TRUE, time);
}


/** clipboard functions **/

static void realize(GtkWidget *w)
/* 'realize' handler */
{
}


static gint expose_event(GtkWidget *w, cairo_t *e)
/* 'expose_event' handler */
{
    static int first_time = 1;

    if (!first_time || mpdm_size(mpdm_get_wcs(MP, L"docs"))) {
        gtk_drv_render(mp_active(), 0);
        first_time = 0;
    }

    return FALSE;
}


static gint configure_event(GtkWidget *w, GdkEventConfigure *event)
/* 'configure_event' handler */
{
    static GdkEventConfigure o;

    if (memcmp(&o, event, sizeof(o)) != 0) {
        memcpy(&o, event, sizeof(o));

        update_window_size();
        gtk_widget_queue_draw(GTK_WIDGET(area));

        gtk_window_get_size(GTK_WINDOW(window), &ls_w, &ls_h);
    }

    return TRUE;
}


static mpdm_t gtk_drv_clip_to_sys(mpdm_t a, mpdm_t ctxt)
/* driver-dependent mp to system clipboard */
{
    GtkClipboard *clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    mpdm_t d = mpdm_join_wcs(mpdm_get_wcs(MP, L"clipboard"), L"\n");
    char *ptr = v_to_utf8(d);

    gtk_clipboard_set_text(clip, ptr, -1);
    g_free(ptr);

    return NULL;
}


static mpdm_t gtk_drv_sys_to_clip(mpdm_t a, mpdm_t ctxt)
/* driver-dependent system to mp clipboard */
{
    GtkClipboard *clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    gchar * clip_txt = gtk_clipboard_wait_for_text(clip);

    if (clip_txt) {
        mpdm_t v = MPDM_MBS(clip_txt);

        /* split and set as the clipboard */
        mpdm_set_wcs(MP, mpdm_split_wcs(v, L"\n"), L"clipboard");
        mpdm_set_wcs(MP, MPDM_I(0), L"clipboard_vertical");
    }

    return NULL;
}


/** interface functions **/

static mpdm_t retrieve_form_data(mpdm_t form_args, GtkWidget **form_widgets)
/* called when confirmation of a form */
{
    int n;
    mpdm_t ret = MPDM_A(mpdm_size(form_args));

    for (n = 0; n < mpdm_size(form_args); n++) {
        GtkWidget *widget = form_widgets[n];
        mpdm_t w = mpdm_get_i(form_args, n);
        wchar_t *wptr = mpdm_string(mpdm_get_wcs(w, L"type"));
        mpdm_t v = NULL;

        if (wcscmp(wptr, L"text") == 0 || wcscmp(wptr, L"password") == 0) {
            char *ptr;
            GtkWidget *gw = widget;
            mpdm_t h;

            if (wcscmp(wptr, L"text") == 0)
#if CONFOPT_GTK == 2
                gw = GTK_COMBO(widget)->entry;
#endif
#if CONFOPT_GTK == 3
                gw = gtk_bin_get_child(GTK_BIN(widget));
#endif

            if ((ptr = gtk_editable_get_chars(GTK_EDITABLE(gw), 0, -1)) != NULL) {
                v = MPDM_MBS(ptr);
                g_free(ptr);
            }

            mpdm_ref(v);

            /* if it has history, fill it */
            if (v && (h = mpdm_get_wcs(w, L"history")) && mpdm_cmp_wcs(v, L"")) {
                h = mp_get_history(h);

                if (mpdm_cmp(v, mpdm_get_i(h, -1)) != 0)
                    mpdm_push(h, v);
            }

            mpdm_unrefnd(v);
        }
        else
        if (wcscmp(wptr, L"checkbox") == 0) {
            v = MPDM_I(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
        }
        else
        if (wcscmp(wptr, L"list") == 0) {
            GtkWidget *list = gtk_bin_get_child(GTK_BIN(widget));
            GtkTreeSelection *selection =
                              gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
            GList *selected = gtk_tree_selection_get_selected_rows(selection, NULL);
            GtkTreePath *path = selected->data;

            v = MPDM_I(gtk_tree_path_get_indices(path)[0]);
            gtk_tree_path_free(path);
            g_list_free(selected);
        }

        mpdm_set_i(ret, v, n);
    }

    return ret;
}


static gint timer_callback(gpointer data)
{
    mpdm_t v;

    if ((v = mpdm_get_wcs(MP, L"timer_func"))) {
        mpdm_void(mpdm_exec(v, NULL, NULL));
        gtk_widget_queue_draw(GTK_WIDGET(area));
    }

    return TRUE;
}


/** dialog functions **/

static mpdm_t gtk_drv_alert(mpdm_t a, mpdm_t ctxt)
/* alert driver function */
{
    gchar *ptr;
    GtkWidget *dlg;

    /* 1# arg: prompt */
    ptr = v_to_utf8(mpdm_get_i(a, 0));

    dlg = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_MODAL,
                                 GTK_MESSAGE_WARNING, GTK_BUTTONS_OK, "%s", ptr);
    gtk_window_set_title(GTK_WINDOW(dlg), "mp " VERSION);
    g_free(ptr);

    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);

    return NULL;
}


static mpdm_t gtk_drv_confirm(mpdm_t a, mpdm_t ctxt)
/* confirm driver function */
{
    char *ptr;
    GtkWidget *dlg;
    gint response;

    /* 1# arg: prompt */
    ptr = v_to_utf8(mpdm_get_i(a, 0));

    dlg = gtk_message_dialog_new(GTK_WINDOW(window), GTK_DIALOG_MODAL,
                                 GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                                 "%s", ptr);
    gtk_window_set_title(GTK_WINDOW(dlg), "mp " VERSION);
    g_free(ptr);

    gtk_dialog_add_button(GTK_DIALOG(dlg), L("Yes"), 1);
    gtk_dialog_add_button(GTK_DIALOG(dlg), L("No"), 2);
    gtk_dialog_add_button(GTK_DIALOG(dlg), L("Cancel"), 0);

    response = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);

    if (response == GTK_RESPONSE_DELETE_EVENT)
        response = 0;

    return MPDM_I(response);
}


static mpdm_t gtk_drv_form(mpdm_t a, mpdm_t ctxt)
/* 'form' driver function */
{
    GtkWidget *dlg;
    GtkWidget *table;
    GtkWidget *content_area;
    GtkWidget **form_widgets;
    mpdm_t form_args;
    int n;
    mpdm_t ret = NULL;

    /* first argument: list of widgets */
    form_args    = mpdm_get_i(a, 0);
    form_widgets = (GtkWidget **) calloc(mpdm_size(form_args), sizeof(GtkWidget *));

    dlg = gtk_dialog_new_with_buttons("mp " VERSION, GTK_WINDOW(window),
                                      GTK_DIALOG_MODAL,
                                      L("Cancel"), GTK_RESPONSE_CANCEL,
                                      L("OK"), GTK_RESPONSE_OK,
                                      NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
    gtk_container_set_border_width(GTK_CONTAINER(dlg), 5);

    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    gtk_box_set_spacing(GTK_BOX(content_area), 2);

#if CONFOPT_GTK == 2
    table = gtk_table_new(mpdm_size(a), 2, FALSE);
    gtk_table_set_col_spacings(GTK_TABLE(table), 12);
    gtk_table_set_row_spacings(GTK_TABLE(table), 6);
#endif

#if CONFOPT_GTK == 3
    table = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(table), 12);
    gtk_grid_set_row_spacing(GTK_GRID(table), 6);
#endif

    gtk_container_set_border_width(GTK_CONTAINER(table), 5);

    for (n = 0; n < mpdm_size(form_args); n++) {
        mpdm_t w = mpdm_get_i(form_args, n);
        GtkWidget *widget = NULL;
        GtkWidget *label = NULL;
        wchar_t *type;
        char *ptr;
        mpdm_t t;
        int col = 0;

        type = mpdm_string(mpdm_get_wcs(w, L"type"));

        if ((t = mpdm_get_wcs(w, L"label")) != NULL) {
            ptr = v_to_utf8(mpdm_gettext(t));
            label = gtk_label_new(ptr);

#if CONFOPT_GTK == 2
            gtk_misc_set_alignment(GTK_MISC(label), 0, .5);
#endif

#if CONFOPT_GTK == 3
            gtk_label_set_xalign(GTK_LABEL(label), 0.0);
            gtk_label_set_yalign(GTK_LABEL(label), 0.5);
#endif

            g_free(ptr);

            col++;
        }

        t = mpdm_get_wcs(w, L"value");

        if (wcscmp(type, L"text") == 0) {
            GList *combo_items = NULL;
            mpdm_t h;

#if CONFOPT_GTK == 2
            widget = gtk_combo_new();
            gtk_combo_set_use_arrows_always(GTK_COMBO(widget), TRUE);
            gtk_combo_set_case_sensitive(GTK_COMBO(widget), TRUE);
            gtk_entry_set_activates_default(GTK_ENTRY(GTK_COMBO(widget)->entry), TRUE);
#endif

#if CONFOPT_GTK == 3
            widget = gtk_combo_box_text_new_with_entry();
            gtk_entry_set_activates_default(
                        GTK_ENTRY(gtk_bin_get_child(GTK_BIN(widget))), TRUE);
#endif

            gtk_widget_set_size_request(widget, 300, -1);

            if ((h = mpdm_get_wcs(w, L"history")) != NULL) {
                int i;

                /* has history; fill it */
                h = mp_get_history(h);

                for (i = 0; i < mpdm_size(h); i++) {
                    ptr = v_to_utf8(mpdm_get_i(h, i));

                    combo_items = g_list_prepend(combo_items, ptr);

#if CONFOPT_GTK == 3
                    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), ptr);
#endif

                    g_free(ptr);
                }
            }

            if (t != NULL) {
                ptr = v_to_utf8(t);

                combo_items = g_list_prepend(combo_items, ptr);
#if CONFOPT_GTK == 3
                gtk_combo_box_text_prepend_text(GTK_COMBO_BOX_TEXT(widget), ptr);
                gtk_combo_box_set_active(GTK_COMBO_BOX(widget), 0);
#endif

                g_free(ptr);
            }

#if CONFOPT_GTK == 2
            gtk_combo_set_popdown_strings(GTK_COMBO(widget), combo_items);
#endif
            g_list_free(combo_items);
        }
        else
        if (wcscmp(type, L"password") == 0) {
            widget = gtk_entry_new();
            gtk_widget_set_size_request(widget, 300, -1);
            gtk_entry_set_visibility(GTK_ENTRY(widget), FALSE);
            gtk_entry_set_activates_default(GTK_ENTRY(widget), TRUE);
        }
        else
        if (wcscmp(type, L"checkbox") == 0) {
            widget = gtk_check_button_new();

            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget),
                                         mpdm_ival(t) ? TRUE : FALSE);
        }
        else
        if (wcscmp(type, L"list") == 0) {
            GtkWidget *list;
            GtkListStore *list_store;
            GtkCellRenderer *renderer;
            GtkTreeViewColumn *column;
            GtkTreePath *path;
            mpdm_t l;
            gint i;

            if ((i = 450 / mpdm_size(form_args)) < 100)
                i = 100;

            widget = gtk_scrolled_window_new(NULL, NULL);
            gtk_widget_set_size_request(widget, 500, i);
            gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(widget),
                                           GTK_POLICY_NEVER,
                                           GTK_POLICY_AUTOMATIC);
            gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW
                                                (widget), GTK_SHADOW_IN);

            list_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
            list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
            gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), FALSE);

            column = gtk_tree_view_column_new();
            gtk_tree_view_column_set_title(column, "");
            gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

            renderer = gtk_cell_renderer_text_new();
            gtk_tree_view_column_pack_start(column, renderer, FALSE);
            gtk_tree_view_column_set_attributes(column, renderer, "text", 0, NULL);

            gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

            column = gtk_tree_view_column_new();
            gtk_tree_view_column_set_title(column, "");
            gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);

            renderer = gtk_cell_renderer_text_new();
            g_object_set(renderer, "xalign", 1.0, NULL);
            gtk_tree_view_column_pack_start(column, renderer, FALSE);
            gtk_tree_view_column_set_attributes(column, renderer, "text", 1, NULL);

            gtk_tree_view_append_column(GTK_TREE_VIEW(list), column);

            gtk_container_add(GTK_CONTAINER(widget), list);

            l = mpdm_get_wcs(w, L"list");

            for (i = 0; i < mpdm_size(l); i++) {
                GtkTreeIter iter;
                char *ptr2;

                ptr = v_to_utf8(mpdm_get_i(l, i));

                /* if there is a tab inside the text,
                   split in two columns */
                if ((ptr2 = strchr(ptr, '\t')) == NULL)
                    ptr2 = "";
                else {
                    *ptr2 = '\0';
                    ptr2++;
                }

                gtk_list_store_append(list_store, &iter);
                gtk_list_store_set(list_store, &iter, 0, ptr, 1, ptr2, -1);
                g_free(ptr);
            }

            /* initial position */
            i = mpdm_ival(t);

            path = gtk_tree_path_new_from_indices(i, -1);
            gtk_tree_view_set_cursor(GTK_TREE_VIEW(list), path, NULL,
                                     FALSE);
            gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(list), path, NULL,
                                    FALSE, 0, 0);
            gtk_tree_path_free(path);

            g_signal_connect_swapped(G_OBJECT(list), "row-activated",
                                     G_CALLBACK(gtk_window_activate_default), dlg);
        }

        if (widget != NULL) {
            form_widgets[n] = widget;

#if CONFOPT_GTK == 2
            if (label)
                gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, n, n + 1);

            gtk_table_attach_defaults(GTK_TABLE(table), widget, col, 2, n, n + 1);
#endif

#if CONFOPT_GTK == 3
            if (label)
                gtk_grid_attach(GTK_GRID(table), label, 0, n, 1, 1);

            if (mpdm_size(form_args) == 1)
                gtk_grid_attach(GTK_GRID(table), widget, 0, n + 1, 1, 1);
            else
                gtk_grid_attach(GTK_GRID(table), widget, 1, n, 1, 1);
#endif
        }
    }

    gtk_widget_show_all(table);

    gtk_box_pack_start(GTK_BOX(content_area), table, TRUE, TRUE, 0);

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_OK)
        ret = retrieve_form_data(form_args, form_widgets);

    gtk_widget_destroy(dlg);

    free(form_widgets);

    return ret;
}


enum {
    FC_OPEN,
    FC_SAVE,
    FC_FOLDER
};


static mpdm_t run_filechooser(mpdm_t a, int type)
/* openfile driver function */
{
    GtkWidget *dlg;
    char *ptr;
    mpdm_t ret = NULL;
    gint response;

    /* 1# arg: prompt */
    ptr = v_to_utf8(mpdm_get_i(a, 0));

    switch (type) {
    case FC_OPEN:
        dlg = gtk_file_chooser_dialog_new(ptr, GTK_WINDOW(window),
                                          GTK_FILE_CHOOSER_ACTION_OPEN,
                                          L("Cancel"),
                                          GTK_RESPONSE_CANCEL,
                                          L("OK"), GTK_RESPONSE_OK,
                                          NULL);
        break;

    case FC_SAVE:
        dlg = gtk_file_chooser_dialog_new(ptr, GTK_WINDOW(window),
                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                          L("Cancel"),
                                          L("Cancel"), L("OK"),
                                          GTK_RESPONSE_OK, NULL);
        gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER
                                                       (dlg), TRUE);
        break;

    case FC_FOLDER:
        dlg = gtk_file_chooser_dialog_new(ptr, GTK_WINDOW(window),
                                          GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                          L("Cancel"),
                                          GTK_RESPONSE_CANCEL,
                                          L("OK"), GTK_RESPONSE_OK,
                                          NULL);
        break;
    }

    g_free(ptr);

    /* override stupid GTK3 "optimal" current folder */
    char tmp[2048];
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), getcwd(tmp, sizeof(tmp)));

    gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dlg), TRUE);
    response = gtk_dialog_run(GTK_DIALOG(dlg));

    if (response == GTK_RESPONSE_OK) {
        gchar *filename;
        gchar *utf8name;

        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        utf8name = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
        g_free(filename);

        ret = MPDM_MBS(utf8name);
        g_free(utf8name);
    }

    gtk_widget_destroy(dlg);

    return ret;
}


static mpdm_t gtk_drv_openfile(mpdm_t a, mpdm_t ctxt)
/* openfile driver function */
{
    return run_filechooser(a, FC_OPEN);
}


static mpdm_t gtk_drv_savefile(mpdm_t a, mpdm_t ctxt)
/* savefile driver function */
{
    return run_filechooser(a, FC_SAVE);
}


static mpdm_t gtk_drv_openfolder(mpdm_t a, mpdm_t ctxt)
/* openfolder driver function */
{
    return run_filechooser(a, FC_FOLDER);
}


static mpdm_t gtk_drv_update_ui(mpdm_t a, mpdm_t ctxt)
{
    build_fonts();
    build_colors();
    build_menu();

    gtk_widget_queue_draw(GTK_WIDGET(area));

    return NULL;
}


static mpdm_t gtk_drv_timer(mpdm_t a, mpdm_t ctxt)
{
    static guint prev = 0;
    int msecs = mpdm_ival(mpdm_get_i(a, 0));
    mpdm_t func = mpdm_get_i(a, 1);

    if (prev)
        g_source_remove(prev);

    mpdm_set_wcs(MP, func, L"timer_func");

    /* if msecs and func are set, program timer */
    if (msecs > 0 && func != NULL)
        prev = g_timeout_add(msecs, timer_callback, NULL);

    return NULL;
}


static mpdm_t gtk_drv_busy(mpdm_t a, mpdm_t ctxt)
{
    int onoff = mpdm_ival(mpdm_get_i(a, 0));

    gdk_window_set_cursor(gtk_widget_get_window(window),
#if CONFOPT_GTK == 2
                          gdk_cursor_new(
#else
                          gdk_cursor_new_for_display(gdk_display_get_default(),
#endif
                          onoff ? GDK_WATCH : GDK_LEFT_PTR));

    while (gtk_events_pending())
        gtk_main_iteration();

    return NULL;
}


static mpdm_t gtk_drv_main_loop(mpdm_t a, mpdm_t ctxt)
/* main loop */
{
    if (!mp_exit_requested) {
        gtk_drv_render(mp_active(), 0);

        gtk_main();
    }

    return NULL;
}


static mpdm_t gtk_drv_shutdown(mpdm_t a, mpdm_t ctxt)
/* shutdown */
{
    mpdm_t v;

    v = mpdm_get_wcs(MP, L"state");
    v = mpdm_set_wcs(v, MPDM_O(), L"window");
    mpdm_set_wcs(v, MPDM_I(ls_x), L"x");
    mpdm_set_wcs(v, MPDM_I(ls_y), L"y");
    mpdm_set_wcs(v, MPDM_I(ls_w), L"w");
    mpdm_set_wcs(v, MPDM_I(ls_h), L"h");

    if ((v = mpdm_get_wcs(MP, L"exit_message")) != NULL) {
        mpdm_write_wcs(stdout, mpdm_string(v));
        printf("\n");
    }

    return NULL;
}


static mpdm_t gtk_drv_menu(void)
{
    if (!gtk_widget_get_visible(menu_bar))
        gtk_widget_show(menu_bar);

    return NULL;
}


static void gtk_register_functions(void)
{
    mpdm_t drv;

    drv = mpdm_get_wcs(mpdm_root(), L"mp_drv");
    mpdm_set_wcs(drv, MPDM_X(gtk_drv_main_loop),    L"main_loop");
    mpdm_set_wcs(drv, MPDM_X(gtk_drv_shutdown),     L"shutdown");
    mpdm_set_wcs(drv, MPDM_X(gtk_drv_clip_to_sys),  L"clip_to_sys");
    mpdm_set_wcs(drv, MPDM_X(gtk_drv_sys_to_clip),  L"sys_to_clip");
    mpdm_set_wcs(drv, MPDM_X(gtk_drv_update_ui),    L"update_ui");
    mpdm_set_wcs(drv, MPDM_X(gtk_drv_timer),        L"timer");
    mpdm_set_wcs(drv, MPDM_X(gtk_drv_busy),         L"busy");
    mpdm_set_wcs(drv, MPDM_X(gtk_drv_alert),        L"alert");
    mpdm_set_wcs(drv, MPDM_X(gtk_drv_confirm),      L"confirm");
    mpdm_set_wcs(drv, MPDM_X(gtk_drv_openfile),     L"openfile");
    mpdm_set_wcs(drv, MPDM_X(gtk_drv_savefile),     L"savefile");
    mpdm_set_wcs(drv, MPDM_X(gtk_drv_form),         L"form");
    mpdm_set_wcs(drv, MPDM_X(gtk_drv_openfolder),   L"openfolder");
    mpdm_set_wcs(drv, MPDM_X(gtk_drv_menu),         L"menu");
}


static mpdm_t gtk_drv_startup(mpdm_t a, mpdm_t ctxt)
/* driver initialization */
{
    GtkWidget *vbox;
    GtkWidget *hbox;
#if CONFOPT_GTK == 2
    GdkPixmap *pixmap;
    GdkPixmap *mask;
#endif
#if CONFOPT_GTK == 3
    GdkPixbuf *pixmap;
#endif
    GdkDisplay *display;
    GdkMonitor *monitor;
    GdkRectangle monitor_one_size;
    mpdm_t v;
    int w, h;
    GtkTargetEntry targets[] = {
        {"text/plain", 0, 0},
        {"text/uri-list", 0, 1}
    };

    gtk_register_functions();

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_title(GTK_WINDOW(window), "mp " VERSION);

    /* get real screen and pick a usable size for the main area */
    display = gdk_display_get_default();

    monitor = gdk_display_get_monitor(display, 0);

    gdk_monitor_get_geometry(monitor, &monitor_one_size);
        
    w = (monitor_one_size.width * 3) / 4;
    h = (monitor_one_size.height * 2) / 3;

    v = mpdm_get_wcs(MP, L"state");
    if ((v = mpdm_get_wcs(v, L"window")) == NULL) {
        v = mpdm_set_wcs(mpdm_get_wcs(MP, L"state"), MPDM_O(), L"window");
        mpdm_set_wcs(v, MPDM_I(0), L"x");
        mpdm_set_wcs(v, MPDM_I(0), L"y");
        mpdm_set_wcs(v, MPDM_I(w), L"w");
        mpdm_set_wcs(v, MPDM_I(h), L"h");
    }

    gtk_window_move(GTK_WINDOW(window),
        mpdm_ival(mpdm_get_wcs(v, L"x")), mpdm_ival(mpdm_get_wcs(v, L"y")));
    gtk_window_set_default_size(GTK_WINDOW(window),
        mpdm_ival(mpdm_get_wcs(v, L"w")), mpdm_ival(mpdm_get_wcs(v, L"h")));

    g_signal_connect(G_OBJECT(window), "delete_event",
                     G_CALLBACK(delete_event), NULL);

    g_signal_connect(G_OBJECT(window), "destroy",
                     G_CALLBACK(destroy), NULL);

    /* file tabs */
    file_tabs = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(file_tabs), GTK_POS_TOP);

#if CONFOPT_GTK == 2
    GTK_WIDGET_UNSET_FLAGS(file_tabs, GTK_CAN_FOCUS);
#endif
#if CONFOPT_GTK == 3
    gtk_widget_set_can_focus(file_tabs, FALSE);
#endif
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(file_tabs), 1);

#if CONFOPT_GTK == 2
    vbox = gtk_vbox_new(FALSE, 2);
#endif
#if CONFOPT_GTK == 3
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
#endif
    gtk_container_add(GTK_CONTAINER(window), vbox);

    build_menu();

#if CONFOPT_GTK == 2
    hbox = gtk_hbox_new(FALSE, 0);
#endif
#if CONFOPT_GTK == 3
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
#endif
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), menu_bar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), file_tabs, FALSE, FALSE, 0);

    gtk_notebook_popup_enable(GTK_NOTEBOOK(file_tabs));

    /* horizontal box holding the text and the scrollbar */
#if CONFOPT_GTK == 2
    hbox = gtk_hbox_new(FALSE, 2);
#endif
#if CONFOPT_GTK == 3
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
#endif
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

    /* the Minimum Profit area */
    area = gtk_drawing_area_new();
    gtk_box_pack_start(GTK_BOX(hbox), area, TRUE, TRUE, 0);
    gtk_widget_set_events(GTK_WIDGET(area), GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK
                          | GDK_LEAVE_NOTIFY_MASK
#if CONFOPT_GTK == 3
                          | GDK_SMOOTH_SCROLL_MASK
#endif
    );

    g_signal_connect(G_OBJECT(area), "configure_event",
                     G_CALLBACK(configure_event), NULL);

#if CONFOPT_GTK == 2
    g_signal_connect(G_OBJECT(area), "expose_event",
#endif
#if CONFOPT_GTK == 3
    g_signal_connect(G_OBJECT(area), "draw",
#endif
                     G_CALLBACK(expose_event), NULL);

    g_signal_connect(G_OBJECT(area), "realize", G_CALLBACK(realize), NULL);

    g_signal_connect(G_OBJECT(window), "key_press_event",
                     G_CALLBACK(key_press_event), NULL);

#if 0
    g_signal_connect(G_OBJECT(window), "key_release_event",
                     G_CALLBACK(key_release_event), NULL);
#endif

    g_signal_connect(G_OBJECT(area), "button_press_event",
                     G_CALLBACK(button_press_event), NULL);

    g_signal_connect(G_OBJECT(area), "button_release_event",
                     G_CALLBACK(button_release_event), NULL);

    g_signal_connect(G_OBJECT(area), "motion_notify_event",
                     G_CALLBACK(motion_notify_event), NULL);

    g_signal_connect(G_OBJECT(area), "scroll_event",
                     G_CALLBACK(scroll_event), NULL);

    gtk_drag_dest_set(area, GTK_DEST_DEFAULT_ALL, targets,
                      sizeof(targets) / sizeof(GtkTargetEntry),
                      GDK_ACTION_COPY);
    g_signal_connect(G_OBJECT(area), "drag_data_received",
                     G_CALLBACK(drag_data_received), NULL);

    g_signal_connect(G_OBJECT(file_tabs), "switch_page",
                     G_CALLBACK(switch_page), NULL);

    /* the scrollbar */
#if CONFOPT_GTK == 2
    scrollbar = gtk_vscrollbar_new(NULL);
#endif
#if CONFOPT_GTK == 3
    scrollbar = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, NULL);
#endif
    gtk_box_pack_start(GTK_BOX(hbox), scrollbar, FALSE, FALSE, 0);

    g_signal_connect(G_OBJECT
                     (gtk_range_get_adjustment(GTK_RANGE(scrollbar))),
                     "value_changed", G_CALLBACK(value_changed), NULL);

    /* the status bar */
    status = gtk_label_new("mp " VERSION);
    gtk_box_pack_start(GTK_BOX(vbox), status, FALSE, FALSE, 0);

#if CONFOPT_GTK == 2
    gtk_misc_set_alignment(GTK_MISC(status), 0, .5);
#endif
#if CONFOPT_GTK == 3
    gtk_label_set_xalign(GTK_LABEL(status), 0.0);
    gtk_label_set_yalign(GTK_LABEL(status), 0.5);
#endif

    gtk_label_set_justify(GTK_LABEL(status), GTK_JUSTIFY_LEFT);

    gtk_widget_show_all(window);

    /* set application icon */
#if CONFOPT_GTK == 2
    pixmap = gdk_pixmap_create_from_xpm_d(window->window,
                                          &mask, NULL, mp_xpm);
    gdk_window_set_icon(window->window, NULL, pixmap, mask);
#endif
#if CONFOPT_GTK == 3
    pixmap = gdk_pixbuf_new_from_xpm_data((const char **)mp_xpm);
    gtk_window_set_icon(GTK_WINDOW(window), pixmap);
#endif

    build_colors();

    if ((v = mpdm_get_wcs(MP, L"config")) != NULL &&
        mpdm_ival(mpdm_get_wcs(v, L"maximize")) > 0)
        maximize = 1;

//    gtk_widget_hide(menu_bar);

    return NULL;
}

int gtk_drv_detect(int *argc, char ***argv)
{
    int n, ret = 1;

    for (n = 0; n < *argc; n++) {
        if (strcmp(argv[0][n], "-txt") == 0)
            ret = 0;
    }

    if (ret) {
        if (gtk_init_check(argc, argv)) {
            mpdm_t drv;

            drv = mpdm_set_wcs(mpdm_root(), MPDM_O(), L"mp_drv");

#if CONFOPT_GTK == 3
            mpdm_set_wcs(drv, MPDM_S(L"gtk3"), L"id");
#else
            mpdm_set_wcs(drv, MPDM_S(L"gtk2"), L"id");
#endif
            mpdm_set_wcs(drv, MPDM_X(gtk_drv_startup), L"startup");
        }
        else
            ret = 0;
    }

    return ret;
}

#endif                          /* CONFOPT_GTK */
