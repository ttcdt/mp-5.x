/*

    Minimum Profit - A Text Editor
    KDE4 driver.

    ttcdt <dev@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

/* override auto-generated definition in config.h */
extern "C" int kde4_drv_detect(void *p);

#include "config.h"

#include <stdio.h>
#include <wchar.h>
#include <unistd.h>
#include "mpdm.h"
#include "mpsl.h"
#include "mp.h"
#include "mp.xpm"

#include <QtGui>

#include <KApplication>
#include <KAboutData>
#include <KCmdLineArgs>

#include <KMainWindow>
#include <KMenuBar>
#include <KStatusBar>
#include <KMenu>
#include <KTabBar>

#include <KVBox>
#include <KHBox>

#include <KDialog>
#include <KMessageBox>
#include <KFileDialog>
#include <KUrl>

/** data **/

class MPWindow : public KMainWindow
{
public:
    MPWindow(QWidget * parent = 0);
    bool queryExit(void);
    bool event(QEvent * event);
};

/* global data */
KApplication *app;
MPWindow *window;
KMenuBar *menubar;
KStatusBar *statusbar;
KTabBar *file_tabs;

#define MENU_CLASS KMenu

#include "mpv_qk_common.cpp"


/** MPWindow methods **/

MPWindow::MPWindow(QWidget * parent) : KMainWindow(parent)
{
    menubar = this->menuBar();
    build_menu();

    statusbar = this->statusBar();
    statusbar->insertItem("mp " VERSION, 0);

    /* the full container */
    KVBox *vb = new KVBox(this);

    file_tabs = new KTabBar(vb);
    file_tabs->setFocusPolicy(Qt::NoFocus);

    KHBox *hb = new KHBox(vb);

    /* main area */
    area = new MPArea(hb);

    setCentralWidget(vb);

    connect(area->scrollbar, SIGNAL(valueChanged(int)),
            area, SLOT(from_scrollbar(int)));

    connect(file_tabs, SIGNAL(currentChanged(int)),
            area, SLOT(from_filetabs(int)));

    connect(menubar, SIGNAL(triggered(QAction *)),
            area, SLOT(from_menu(QAction *)));

    this->setWindowIcon(QIcon(QPixmap(mp_xpm)));

    this->setAutoSaveSettings(QLatin1String("MinimumProfit"), true);
}


bool MPWindow::queryExit(void)
{
    mp_process_event(MPDM_S(L"close-window"));

    this->saveAutoSaveSettings();

    return mp_exit_requested ? true : false;
}


bool MPWindow::event(QEvent * event)
{
    /* do the processing */
    bool r = QWidget::event(event);

    if (mp_exit_requested) {
        this->saveAutoSaveSettings();
        exit(0);
    }

    return r;
}


/** driver functions **/

static mpdm_t kde4_drv_update_ui(mpdm_t a, mpdm_t ctxt)
{
    return qt4_drv_update_ui(a, ctxt);
}


static mpdm_t kde4_drv_alert(mpdm_t a, mpdm_t ctxt)
/* alert driver function */
{
    /* 1# arg: prompt */
    KMessageBox::information(0, v_to_qstring(mpdm_aget(a, 0)),
                             i18n("mp " VERSION));

    return NULL;
}

static mpdm_t kde4_drv_confirm(mpdm_t a, mpdm_t ctxt)
/* confirm driver function */
{
    int r;

    /* 1# arg: prompt */
    r = KMessageBox::questionYesNoCancel(0,
                                         v_to_qstring(mpdm_aget(a, 0)),
                                         i18n("mp" VERSION));

    switch (r) {
    case KMessageBox::Yes:
        r = 1;
        break;

    case KMessageBox::No:
        r = 2;
        break;

    case KMessageBox::Cancel:
        r = 0;
        break;
    }

    return MPDM_I(r);
}


static mpdm_t kde4_drv_openfile(mpdm_t a, mpdm_t ctxt)
{
    QString r;
    char tmp[128];

    getcwd(tmp, sizeof(tmp));

    /* 1# arg: prompt */
    r = KFileDialog::getOpenFileName(KUrl::fromPath(tmp), "*", 0,
                                     v_to_qstring(mpdm_aget(a, 0)));

    return qstring_to_v(r);
}


static mpdm_t kde4_drv_savefile(mpdm_t a, mpdm_t ctxt)
{
    QString r;
    char tmp[128];

    getcwd(tmp, sizeof(tmp));

    /* 1# arg: prompt */
    r = KFileDialog::getSaveFileName(KUrl::fromPath(tmp), "*", 0,
                                     v_to_qstring(mpdm_aget(a, 0)));

    return qstring_to_v(r);
}


static mpdm_t kde4_drv_form(mpdm_t a, mpdm_t ctxt)
{
    int n;
    mpdm_t widget_list;
    QWidget *qlist[100];
    mpdm_t r = NULL;

    KDialog *dialog = new KDialog(window);

    dialog->setModal(true);
    dialog->setButtons(KDialog::Ok | KDialog::Cancel);

    widget_list = mpdm_aget(a, 0);

    KVBox *vb = new KVBox(dialog);
    dialog->setMainWidget(vb);

    for (n = 0; n < mpdm_size(widget_list); n++) {
        mpdm_t w = mpdm_aget(widget_list, n);
        wchar_t *type;
        mpdm_t t;
        KHBox *hb = new KHBox(vb);

        type = mpdm_string(mpdm_hget_s(w, L"type"));

        if ((t = mpdm_hget_s(w, L"label")) != NULL) {
            QLabel *ql = new QLabel(hb);
            ql->setText(v_to_qstring(mpdm_gettext(t)));
        }

        t = mpdm_hget_s(w, L"value");

        if (wcscmp(type, L"text") == 0) {
            mpdm_t h;
            QComboBox *ql = new QComboBox(hb);

            ql->setEditable(true);
            ql->setMinimumContentsLength(30);
            ql->setMaxVisibleItems(8);

            if (t != NULL)
                ql->setEditText(v_to_qstring(t));

            qlist[n] = ql;

            if ((h = mpdm_hget_s(w, L"history")) != NULL) {
                int i;

                /* has history; fill it */
                h = mp_get_history(h);

                for (i = mpdm_size(h) - 1; i >= 0; i--)
                    ql->addItem(v_to_qstring(mpdm_aget(h, i)));
            }
        }
        else
        if (wcscmp(type, L"password") == 0) {
            QLineEdit *ql = new QLineEdit(hb);

            ql->setEchoMode(QLineEdit::Password);

            qlist[n] = ql;
        }
        else
        if (wcscmp(type, L"checkbox") == 0) {
            QCheckBox *qc = new QCheckBox(hb);

            if (mpdm_ival(t))
                qc->setCheckState(Qt::Checked);

            qlist[n] = qc;
        }
        else
        if (wcscmp(type, L"list") == 0) {
            int i;
            QListWidget *ql = new QListWidget(hb);
            ql->setMinimumWidth(480);

            /* use a monospaced font */
            ql->setFont(QFont(QString("Mono")));

            mpdm_t l = mpdm_hget_s(w, L"list");

            for (i = 0; i < mpdm_size(l); i++)
                ql->addItem(v_to_qstring(mpdm_aget(l, i)));

            ql->setCurrentRow(mpdm_ival(t));

            qlist[n] = ql;
        }

        if (n == 0)
            qlist[n]->setFocus(Qt::OtherFocusReason);
    }

    if (dialog->exec()) {
        r = MPDM_A(mpdm_size(widget_list));

        /* fill the return values */
        for (n = 0; n < mpdm_size(widget_list); n++) {
            mpdm_t w = mpdm_aget(widget_list, n);
            mpdm_t v = NULL;
            wchar_t *type;

            type = mpdm_string(mpdm_hget_s(w, L"type"));

            if (wcscmp(type, L"text") == 0) {
                mpdm_t h;
                QComboBox *ql = (QComboBox *) qlist[n];

                v = mpdm_ref(qstring_to_v(ql->currentText()));

                /* if it has history, add to it */
                if (v && (h = mpdm_hget_s(w, L"history")) && mpdm_cmp_wcs(v, L"")) {
                    h = mp_get_history(h);

                    if (mpdm_cmp(v, mpdm_aget(h, -1)) != 0)
                        mpdm_push(h, v);
                }

                mpdm_unrefnd(v);
            }
            else
            if (wcscmp(type, L"password") == 0) {
                QLineEdit *ql = (QLineEdit *) qlist[n];

                v = qstring_to_v(ql->text());
            }
            else
            if (wcscmp(type, L"checkbox") == 0) {
                QCheckBox *qb = (QCheckBox *) qlist[n];

                v = MPDM_I(qb->checkState() == Qt::Checked);
            }
            else
            if (wcscmp(type, L"list") == 0) {
                QListWidget *ql = (QListWidget *) qlist[n];

                v = MPDM_I(ql->currentRow());
            }

            mpdm_aset(r, v, n);
        }
    }

    return r;
}


static mpdm_t kde4_drv_busy(mpdm_t a, mpdm_t ctxt)
{
    return qt4_drv_busy(a, ctxt);
}


static mpdm_t kde4_drv_main_loop(mpdm_t a, mpdm_t ctxt)
{
    return qt4_drv_main_loop(a, ctxt);
}


static mpdm_t kde4_drv_shutdown(mpdm_t a, mpdm_t ctxt)
{
    return qt4_drv_shutdown(a, ctxt);
}


static mpdm_t kde4_drv_clip_to_sys(mpdm_t a, mpdm_t ctxt)
{
    return qt4_drv_clip_to_sys(a, ctxt);
}


static mpdm_t kde4_drv_sys_to_clip(mpdm_t a, mpdm_t ctxt)
{
    return qt4_drv_sys_to_clip(a, ctxt);
}


static mpdm_t kde4_drv_idle(mpdm_t a, mpdm_t ctxt)
{
    return qt4_drv_idle(a, ctxt);
}

static void kde4_register_functions(void)
{
    mpdm_t drv;

    drv = mpdm_hget_s(mpdm_root(), L"mp_drv");
    mpdm_hset_s(drv, L"main_loop",   MPDM_X(kde4_drv_main_loop));
    mpdm_hset_s(drv, L"shutdown",    MPDM_X(kde4_drv_shutdown));
    mpdm_hset_s(drv, L"clip_to_sys", MPDM_X(kde4_drv_clip_to_sys));
    mpdm_hset_s(drv, L"sys_to_clip", MPDM_X(kde4_drv_sys_to_clip));
    mpdm_hset_s(drv, L"update_ui",   MPDM_X(kde4_drv_update_ui));
    mpdm_hset_s(drv, L"idle",        MPDM_X(kde4_drv_idle));
    mpdm_hset_s(drv, L"busy",        MPDM_X(kde4_drv_busy));
    mpdm_hset_s(drv, L"alert",       MPDM_X(kde4_drv_alert));
    mpdm_hset_s(drv, L"confirm",     MPDM_X(kde4_drv_confirm));
    mpdm_hset_s(drv, L"openfile",    MPDM_X(kde4_drv_openfile));
    mpdm_hset_s(drv, L"savefile",    MPDM_X(kde4_drv_savefile));
    mpdm_hset_s(drv, L"form",        MPDM_X(kde4_drv_form));
}


static mpdm_t kde4_drv_startup(mpdm_t a, mpdm_t ctxt)
/* driver initialization */
{
    kde4_register_functions();

    qk_build_font(1);
    qk_build_colors();

    window = new MPWindow();
    window->show();

    return NULL;
}

extern "C" Display *XOpenDisplay(char *);

extern "C" int kde4_drv_detect(void *p)
{
    KCmdLineOptions opts;
    int ret = 1;
    int64_t c = 0;
    mpdm_t v, argv = (mpdm_t) p;

    while (mpdm_iterator(argv, &c, &v, NULL)) {
        if (mpdm_cmp_wcs(v, L"-txt") == 0)
            ret = 0;
    }

    if (ret) {
        Display *x11_display;

        /* try connecting directly to the Xserver */
        if ((x11_display = XOpenDisplay((char *) NULL))) {
            mpdm_t drv;

            KAboutData aboutData("mp", 0,
                         ki18n("Minimum Profit"), VERSION,
                         ki18n("A programmer's text editor"),
                         ki18n("ttcdt et al."),
                         ki18n(""),
                         "https://triptico.com", "dev@triptico.com");

            KCmdLineArgs::init(*argc, *argv, &aboutData);

            /* command line options should be inserted here (I don't like this) */
            opts.add("t {tag}",         ki18n("Edits the file where tag is defined"));
            opts.add("e {mpsl_code}",   ki18n("Executes MPSL code"));
            opts.add("f {mpsl_script}", ki18n("Executes MPSL script file"));
            opts.add("d {directory}",   ki18n("Sets working directory"));
            opts.add("x {file}",        ki18n("Open file in the hexadecimal viewer"));
            opts.add(" +NNN",           ki18n("Moves to line number NNN of last file"));
            opts.add("txt",             ki18n("Use text mode instead of GUI"));
            opts.add("+[file(s)]",      ki18n("Documents to open"));

            KCmdLineArgs::addCmdLineOptions(opts);

            /* this is where it crashes if no X server */
            app = new KApplication(x11_display);

            drv = mpdm_hset_s(mpdm_root(), L"mp_drv", MPDM_H(0));

            mpdm_hset_s(drv, L"id",      MPDM_S(L"kde4"));
            mpdm_hset_s(drv, L"startup", MPDM_X(kde4_drv_startup));
        }
        else
            ret = 0;
    }

    return ret;
}
