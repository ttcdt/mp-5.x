/*

    Minimum Profit - A Text Editor
    Code common to Qt4 and KDE4 drivers.

    ttcdt <dev@triptico.com> et al.

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

class MPArea : public QWidget
{
    Q_OBJECT

public:
    MPArea(QWidget * parent = 0);
    void inputMethodEvent(QInputMethodEvent * event);
    void keyPressEvent(QKeyEvent * event);
    void keyReleaseEvent(QKeyEvent * event);
    void mousePressEvent(QMouseEvent * event);
    void mouseDoubleClickEvent(QMouseEvent * event);
    void mouseReleaseEvent(QMouseEvent * event);
    void mouseMoveEvent(QMouseEvent * event);
    void wheelEvent(QWheelEvent * event);
    void dragEnterEvent(QDragEnterEvent * event);
    void dropEvent(QDropEvent * event);
    bool event(QEvent * event);

    void draw_scrollbar();
    void draw_status();
    void draw_filetabs();

    QFont *font;

    QScrollBar *scrollbar;
    QLabel *statusbar;
    QTabBar *file_tabs;

    QTimer *timer;

    QPixmap *pixmap;
    int ls_width;
    int ls_height;
    int ignore_scrollbar_signal;
    int mouse_down;
    qreal font_width;
    qreal font_height;

protected:
    void paintEvent(QPaintEvent * event);

public slots:
    void from_scrollbar(int);
    void from_filetabs(int);
    void from_menu(QAction *);
    void from_timer(void);
};


/* hash of qactions to MP actions */
QHash <QAction *, mpdm_t> qaction_to_action;


/** code **/

static mpdm_t qstring_to_v(QString s)
/* converts a QString to an MPDM string */
{
    mpdm_t r = NULL;

    if (s != NULL) {
        int t = s.size();
        wchar_t *wptr = (wchar_t *) calloc((t + 1), sizeof(wchar_t));

        r = MPDM_ENS(wptr, t);

        s.toWCharArray(wptr);
    }

    return r;
}


QString v_to_qstring(mpdm_t s)
/* converts an MPDM string to a QString */
{
    return QString::fromWCharArray(mpdm_string(s));
}


#define MAX_COLORS 100
QPen inks[MAX_COLORS];
QBrush papers[MAX_COLORS];
static bool underlines[MAX_COLORS];
static bool italics[MAX_COLORS];
static int normal_attr = 0;

static void qk_build_colors(void)
/* builds the colors */
{
    mpdm_t colors;
    mpdm_t v, i;
    int n;
    int64_t c;

    /* gets the color definitions and attribute names */
    colors = mpdm_get_wcs(MP, L"colors");

    /* loop the colors */
    n = c = 0;
    while (mpdm_iterator(colors, &c, &v, &i)) {
        int rgbi, rgbp;
        mpdm_t w = mpdm_get_wcs(v, L"gui");

        /* store the 'normal' attribute */
        if (wcscmp(mpdm_string(i), L"normal") == 0)
            normal_attr = n;

        /* store the attr */
        mpdm_set_wcs(v, MPDM_I(n), L"attr");

        rgbi = mpdm_ival(mpdm_get_i(w, 0));
        rgbp = mpdm_ival(mpdm_get_i(w, 1));

        /* flags */
        w = mpdm_get_wcs(v, L"flags");

        if (mpdm_seek_wcs(w, L"reverse", 1) != -1) {
            int t = rgbi;
            rgbi = rgbp;
            rgbp = t;
        }

        underlines[n] = mpdm_seek_wcs(w, L"underline", 1) != -1 ? true : false;

        italics[n] = mpdm_seek_wcs(w, L"italic", 1) != -1 ? true : false;

        inks[n] = QPen(QColor::fromRgbF((float) ((rgbi & 0x00ff0000) >> 16) / 256.0,
                                        (float) ((rgbi & 0x0000ff00) >> 8)  / 256.0,
                                        (float) ((rgbi & 0x000000ff)) / 256.0, 1));

        papers[n] = QBrush(QColor::fromRgbF((float) ((rgbp & 0x00ff0000) >> 16) / 256.0,
                                            (float) ((rgbp & 0x0000ff00) >> 8) / 256.0,
                                            (float) ((rgbp & 0x000000ff)) / 256.0, 1));

        n++;
    }
}


static QFont *qk_build_font(void)
/* (re)builds the font */
{
    QFont *font;
    mpdm_t c;
    mpdm_t w = NULL;
    int font_size       = 10;
    char *font_face     = (char *) "Mono";
    double font_weight  = 0.0;

    if ((c = mpdm_get_wcs(MP, L"config")) != NULL) {
        mpdm_t v;

        if ((v = mpdm_get_wcs(c, L"font_size")) != NULL)
            font_size = mpdm_ival(v);
        else
            mpdm_set_wcs(c, MPDM_I(font_size), L"font_size");

        if ((v = mpdm_get_wcs(c, L"font_weight")) != NULL)
            font_weight = mpdm_rval(v) * 100.0;
        else
            mpdm_set_wcs(c, MPDM_R(font_weight / 100.0), L"font_weight");

        if ((v = mpdm_get_wcs(c, L"font_face")) != NULL) {
            w = mpdm_ref(MPDM_2MBS(mpdm_string(v)));
            font_face = (char *) mpdm_data(w);
        }
        else
            mpdm_set_wcs(c, MPDM_MBS(font_face), L"font_face");
    }

    font = new QFont(QString(font_face), font_size);
    font->setStyleHint(QFont::TypeWriter);

    font->setFixedPitch(true);

    if (font_weight > 0.0)
        font->setWeight((int) font_weight);

    mpdm_unref(w);

    return font;
}


static void qk_build_menu(MENUBAR_CLASS *menubar)
/* builds the menu */
{
    int n;
    mpdm_t m;

    /* gets the current menu */
    m = mpdm_get_wcs(MP, L"menu");

    menubar->clear();

    for (n = 0; n < (int) mpdm_size(m); n++) {
        mpdm_t mi;
        mpdm_t v;
        int i;

        /* get the label */
        mi = mpdm_get_i(m, n);
        v = mpdm_get_i(mi, 0);

        MENU_CLASS *menu = menubar->addMenu(v_to_qstring(mpdm_gettext(v)));

        /* get the items */
        v = mpdm_get_i(mi, 1);

        for (i = 0; i < (int) mpdm_size(v); i++) {
            wchar_t *wptr;
            mpdm_t w = mpdm_get_i(v, i);

            wptr = mpdm_string(w);

            if (*wptr == L'-')
                menu->addSeparator();
            else {
                mpdm_ref(w);
                qaction_to_action[menu->addAction(v_to_qstring(mp_menu_label(w)))] = w;
            }
        }
    }

    menubar->show();
}


/** MPArea methods **/

MPArea::MPArea(QWidget *parent) : QWidget(parent)
{
    font = qk_build_font();

    setAttribute(Qt::WA_InputMethodEnabled, true);

    setAcceptDrops(true);

    timer = new QTimer();
    connect(timer, SIGNAL(timeout()), this, SLOT(from_timer()));

    ls_width    = -1;
    ls_height   = -1;

    scrollbar = new QScrollBar();
    scrollbar->setFocusPolicy(Qt::NoFocus);

    statusbar = new QLabel();

    file_tabs = new QTabBar();
    file_tabs->setFocusPolicy(Qt::NoFocus);

    ignore_scrollbar_signal = 0;
    mouse_down = 0;

    font_width = font_height = -1;
}


bool MPArea::event(QEvent *event)
{
    /* special tab processing */
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = (QKeyEvent *) event;

        if (ke->key() == Qt::Key_Tab) {
            mp_process_event(MPDM_S(L"tab"));
            update();
            return true;
        }
        else
        if (ke->key() == Qt::Key_Backtab) {
            mp_process_event(MPDM_S(L"shift-tab"));
            update();
            return true;
        }
    }

    /* keep normal processing */
    return QWidget::event(event);
}


void MPArea::draw_scrollbar(void)
{
    static int ol  = -1;
    static int ovy = -1;
    static int oty = -1;
    mpdm_t txt     = mpdm_get_wcs(mp_active(), L"txt");
    mpdm_t window  = mpdm_get_wcs(MP, L"window");
    int vy         = mpdm_ival(mpdm_get_wcs(txt, L"vy"));
    int ty         = mpdm_ival(mpdm_get_wcs(window, L"ty"));
    int l          = mpdm_size(mpdm_get_wcs(txt, L"lines")) - ty;

    if (ol != l || ovy != vy || oty != ty) {
        ignore_scrollbar_signal = 1;

        scrollbar->setMinimum(0);
        scrollbar->setMaximum(ol = l);
        scrollbar->setValue(ovy = vy);
        scrollbar->setPageStep(oty = ty);

        ignore_scrollbar_signal = 0;
    }
}


void MPArea::draw_status(void)
{
    statusbar->setText(v_to_qstring(mp_build_status_line()));
}


void MPArea::draw_filetabs(void)
{
    static mpdm_t prev = NULL;
    mpdm_t names;
    int n, i;

    names = mpdm_ref(mp_get_doc_names());

    /* get mp.active_i now, because it can be changed
       from the signal handler */
    i = mpdm_ival(mpdm_get_wcs(MP, L"active_i"));

    /* is the list different from the previous one? */
    if (mpdm_cmp(names, prev) != 0) {
        while (file_tabs->count())
            file_tabs->removeTab(0);

        /* create the new ones */
        for (n = 0; n < (int) mpdm_size(names); n++)
            file_tabs->addTab(v_to_qstring(mpdm_get_i(names, n)));

        /* store for the next time */
        mpdm_store(&prev, names);
    }

    mpdm_unref(names);

    /* set the active one */
    file_tabs->setCurrentIndex(i);
}


void MPArea::paintEvent(QPaintEvent *)
{
    mpdm_t w;
    int n, m, y, yb;
    bool underline = false;
    bool italic = false;
    QPen *epen;
    int is_new = 0;

    if (this->width() != ls_width && this->height() != ls_height) {
        ls_width    = this->width();
        ls_height   = this->height();
        pixmap      = new QPixmap(ls_width, ls_height);
        is_new      = 1;
    }

    epen = new QPen(inks[normal_attr]);
    epen->setStyle(Qt::NoPen);

    QPainter painter(pixmap);

    if (is_new) {
        painter.setPen(*epen);
        painter.setBrush(papers[normal_attr]);
        painter.drawRect(0, 0, ls_width, ls_height);
    }

    font->setUnderline(false);
    painter.setFont(*font);

    QFontMetricsF fm(*font);

    font_width = fm.averageCharWidth();
    font_height = fm.height();

    /* calculate window size */
    w = mpdm_get_wcs(MP, L"window");
    mpdm_set_wcs(w, MPDM_I(this->width() / font_width),   L"tx");
    mpdm_set_wcs(w, MPDM_I(this->height() / font_height), L"ty");

    w = mp_draw(mp_active(), !is_new);

    yb = painter.fontMetrics().ascent() + 1;
    y = 0;

    painter.setBackgroundMode(Qt::OpaqueMode);

    mpdm_ref(w);
    mpdm_push(w, MPDM_A(0));

    for (n = 0; n < (int) mpdm_size(w); n++) {
        mpdm_t l = mpdm_get_i(w, n);
        qreal x = 0;

        if (l != NULL) {
            painter.setPen(*epen);
            painter.setBrush(papers[normal_attr]);
            painter.drawRect(x, y + 1, ls_width - x, font_height);
            italic = false;
            font->setItalic(italic);
            painter.setFont(*font);

            for (m = 0; m < (int) mpdm_size(l); m++) {
                int attr;
                mpdm_t s;

                /* get the attribute and the string */
                attr = mpdm_ival(mpdm_get_i(l, m++));
                s = mpdm_get_i(l, m);

                painter.setPen(inks[attr]);
                painter.setBackground(papers[attr]);

                QString qs = v_to_qstring(s);

                if (underline != underlines[attr]) {
                    underline = underlines[attr];
                    font->setUnderline(underline);
                    painter.setFont(*font);
                }

                if (italic != italics[attr]) {
                    italic = italics[attr];
                    font->setItalic(italic);
                    painter.setFont(*font);
                }

                painter.drawText(x, y + yb, qs);

#ifdef CONFOPT_QT5
                x += fm.horizontalAdvance(qs);
#else
                x += fm.width(qs);
#endif
            }
        }

        y += font_height;
    }

    mpdm_unref(w);

    QPainter painter2(this);
    painter2.drawPixmap(0, 0, ls_width, ls_height, *pixmap);

    draw_filetabs();
    draw_scrollbar();
    draw_status();

    setFocus(Qt::OtherFocusReason);
}


void MPArea::inputMethodEvent(QInputMethodEvent *event)
{
    QString s = event->commitString();

    mp_process_event(qstring_to_v(s));
    update();
}


void MPArea::keyReleaseEvent(QKeyEvent *event)
{
    if (!event->isAutoRepeat()) {
    }
}


void MPArea::keyPressEvent(QKeyEvent *event)
{
    mpdm_t k = NULL;
    wchar_t *ptr = NULL;

    /* set mp.shift_pressed */
    if (event->modifiers() & Qt::ShiftModifier)
        mpdm_set_wcs(MP, MPDM_I(1), L"shift_pressed");

    if (event->modifiers() & Qt::ShiftModifier) {
        switch (event->key()) {
        case Qt::Key_F1:
            ptr = (wchar_t *) L"shift-f1";
            break;
        case Qt::Key_F2:
            ptr = (wchar_t *) L"shift-f2";
            break;
        case Qt::Key_F3:
            ptr = (wchar_t *) L"shift-f3";
            break;
        case Qt::Key_F4:
            ptr = (wchar_t *) L"shift-f4";
            break;
        case Qt::Key_F5:
            ptr = (wchar_t *) L"shift-f5";
            break;
        case Qt::Key_F6:
            ptr = (wchar_t *) L"shift-f6";
            break;
        case Qt::Key_F7:
            ptr = (wchar_t *) L"shift-f7";
            break;
        case Qt::Key_F8:
            ptr = (wchar_t *) L"shift-f8";
            break;
        case Qt::Key_F9:
            ptr = (wchar_t *) L"shift-f9";
            break;
        case Qt::Key_F10:
            ptr = (wchar_t *) L"shift-f10";
            break;
        case Qt::Key_F11:
            ptr = (wchar_t *) L"shift-f11";
            break;
        case Qt::Key_F12:
            ptr = (wchar_t *) L"shift-f12";
            break;
        }
    }    

    if (ptr == NULL && (event->modifiers() & Qt::ControlModifier)) {
        switch (event->key()) {
        case Qt::Key_Up:
            ptr = (wchar_t *) L"ctrl-cursor-up";
            break;
        case Qt::Key_Down:
            ptr = (wchar_t *) L"ctrl-cursor-down";
            break;
        case Qt::Key_Left:
            ptr = (wchar_t *) L"ctrl-cursor-left";
            break;
        case Qt::Key_Right:
            ptr = (wchar_t *) L"ctrl-cursor-right";
            break;
        case Qt::Key_PageUp:
            ptr = (wchar_t *) L"ctrl-page-up";
            break;
        case Qt::Key_PageDown:
            ptr = (wchar_t *) L"ctrl-page-down";
            break;
        case Qt::Key_Home:
            ptr = (wchar_t *) L"ctrl-home";
            break;
        case Qt::Key_End:
            ptr = (wchar_t *) L"ctrl-end";
            break;
        case Qt::Key_Space:
            ptr = (wchar_t *) L"ctrl-space";
            break;
        case Qt::Key_F1:
            ptr = (wchar_t *) L"ctrl-f1";
            break;
        case Qt::Key_F2:
            ptr = (wchar_t *) L"ctrl-f2";
            break;
        case Qt::Key_F3:
            ptr = (wchar_t *) L"ctrl-f3";
            break;
        case Qt::Key_F4:
            ptr = (wchar_t *) L"ctrl-f4";
            break;
        case Qt::Key_F5:
            ptr = (wchar_t *) L"ctrl-f5";
            break;
        case Qt::Key_F6:
            ptr = (wchar_t *) L"ctrl-f6";
            break;
        case Qt::Key_F7:
            ptr = (wchar_t *) L"ctrl-f7";
            break;
        case Qt::Key_F8:
            ptr = (wchar_t *) L"ctrl-f8";
            break;
        case Qt::Key_F9:
            ptr = (wchar_t *) L"ctrl-f9";
            break;
        case Qt::Key_F10:
            ptr = (wchar_t *) L"ctrl-f10";
            break;
        case Qt::Key_F11:
            ptr = (wchar_t *) L"ctrl-f11";
            break;
        case Qt::Key_F12:
            ptr = (wchar_t *) L"ctrl-f12";
            break;
        case 'A':
            ptr = (wchar_t *) L"ctrl-a";
            break;
        case 'B':
            ptr = (wchar_t *) L"ctrl-b";
            break;
        case 'C':
            ptr = (wchar_t *) L"ctrl-c";
            break;
        case 'D':
            ptr = (wchar_t *) L"ctrl-d";
            break;
        case 'E':
            ptr = (wchar_t *) L"ctrl-e";
            break;
        case 'F':
            ptr = (wchar_t *) L"ctrl-f";
            break;
        case 'G':
            ptr = (wchar_t *) L"ctrl-g";
            break;
        case 'H':
            ptr = (wchar_t *) L"ctrl-h";
            break;
        case 'I':
            ptr = (wchar_t *) L"ctrl-i";
            break;
        case 'J':
            ptr = (wchar_t *) L"ctrl-j";
            break;
        case 'K':
            ptr = (wchar_t *) L"ctrl-k";
            break;
        case 'L':
            ptr = (wchar_t *) L"ctrl-l";
            break;
        case 'M':
            ptr = (wchar_t *) L"ctrl-m";
            break;
        case 'N':
            ptr = (wchar_t *) L"ctrl-n";
            break;
        case 'O':
            ptr = (wchar_t *) L"ctrl-o";
            break;
        case 'P':
            ptr = (wchar_t *) L"ctrl-p";
            break;
        case 'Q':
            ptr = (wchar_t *) L"ctrl-q";
            break;
        case 'R':
            ptr = (wchar_t *) L"ctrl-r";
            break;
        case 'S':
            ptr = (wchar_t *) L"ctrl-s";
            break;
        case 'T':
            ptr = (wchar_t *) L"ctrl-t";
            break;
        case 'U':
            ptr = (wchar_t *) L"ctrl-u";
            break;
        case 'V':
            ptr = (wchar_t *) L"ctrl-v";
            break;
        case 'W':
            ptr = (wchar_t *) L"ctrl-w";
            break;
        case 'X':
            ptr = (wchar_t *) L"ctrl-x";
            break;
        case 'Y':
            ptr = (wchar_t *) L"ctrl-y";
            break;
        case 'Z':
            ptr = (wchar_t *) L"ctrl-z";
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            ptr = (wchar_t *) L"ctrl-enter";
            break;

        default:
            break;
        }
    }
    else
    if (ptr == NULL && (event->modifiers() & Qt::AltModifier)) {
        switch (event->key()) {
        case Qt::Key_Up:
            ptr = (wchar_t *) L"alt-cursor-up";
            break;
        case Qt::Key_Down:
            ptr = (wchar_t *) L"alt-cursor-down";
            break;
        case Qt::Key_Left:
            ptr = (wchar_t *) L"alt-cursor-left";
            break;
        case Qt::Key_Right:
            ptr = (wchar_t *) L"alt-cursor-right";
            break;
        case Qt::Key_PageUp:
            ptr = (wchar_t *) L"alt-page-up";
            break;
        case Qt::Key_PageDown:
            ptr = (wchar_t *) L"alt-page-down";
            break;
        case Qt::Key_Home:
            ptr = (wchar_t *) L"alt-home";
            break;
        case Qt::Key_End:
            ptr = (wchar_t *) L"alt-end";
            break;
        case Qt::Key_Space:
            ptr = (wchar_t *) L"alt-space";
            break;
        case Qt::Key_F1:
            ptr = (wchar_t *) L"alt-f1";
            break;
        case Qt::Key_F2:
            ptr = (wchar_t *) L"alt-f2";
            break;
        case Qt::Key_F3:
            ptr = (wchar_t *) L"alt-f3";
            break;
        case Qt::Key_F4:
            ptr = (wchar_t *) L"alt-f4";
            break;
        case Qt::Key_F5:
            ptr = (wchar_t *) L"alt-f5";
            break;
        case Qt::Key_F6:
            ptr = (wchar_t *) L"alt-f6";
            break;
        case Qt::Key_F7:
            ptr = (wchar_t *) L"alt-f7";
            break;
        case Qt::Key_F8:
            ptr = (wchar_t *) L"alt-f8";
            break;
        case Qt::Key_F9:
            ptr = (wchar_t *) L"alt-f9";
            break;
        case Qt::Key_F10:
            ptr = (wchar_t *) L"alt-f10";
            break;
        case Qt::Key_F11:
            ptr = (wchar_t *) L"alt-f11";
            break;
        case Qt::Key_F12:
            ptr = (wchar_t *) L"alt-f12";
            break;
        case 'A':
            ptr = (wchar_t *) L"alt-a";
            break;
        case 'B':
            ptr = (wchar_t *) L"alt-b";
            break;
        case 'C':
            ptr = (wchar_t *) L"alt-c";
            break;
        case 'D':
            ptr = (wchar_t *) L"alt-d";
            break;
        case 'E':
            ptr = (wchar_t *) L"alt-e";
            break;
        case 'F':
            ptr = (wchar_t *) L"alt-f";
            break;
        case 'G':
            ptr = (wchar_t *) L"alt-g";
            break;
        case 'H':
            ptr = (wchar_t *) L"alt-h";
            break;
        case 'I':
            ptr = (wchar_t *) L"alt-i";
            break;
        case 'J':
            ptr = (wchar_t *) L"alt-j";
            break;
        case 'K':
            ptr = (wchar_t *) L"alt-k";
            break;
        case 'L':
            ptr = (wchar_t *) L"alt-l";
            break;
        case 'M':
            ptr = (wchar_t *) L"alt-m";
            break;
        case 'N':
            ptr = (wchar_t *) L"alt-n";
            break;
        case 'O':
            ptr = (wchar_t *) L"alt-o";
            break;
        case 'P':
            ptr = (wchar_t *) L"alt-p";
            break;
        case 'Q':
            ptr = (wchar_t *) L"alt-q";
            break;
        case 'R':
            ptr = (wchar_t *) L"alt-r";
            break;
        case 'S':
            ptr = (wchar_t *) L"alt-s";
            break;
        case 'T':
            ptr = (wchar_t *) L"alt-t";
            break;
        case 'U':
            ptr = (wchar_t *) L"alt-u";
            break;
        case 'V':
            ptr = (wchar_t *) L"alt-v";
            break;
        case 'W':
            ptr = (wchar_t *) L"alt-w";
            break;
        case 'X':
            ptr = (wchar_t *) L"alt-x";
            break;
        case 'Y':
            ptr = (wchar_t *) L"alt-y";
            break;
        case 'Z':
            ptr = (wchar_t *) L"alt-z";
            break;
        case '-':
            ptr = (wchar_t *) L"alt-minus";
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            ptr = (wchar_t *) L"alt-enter";
            break;

        default:
            break;
        }
    }
    else 
    if (ptr == NULL) {
        switch (event->key()) {
        case Qt::Key_Up:
            ptr = (wchar_t *) L"cursor-up";
            break;
        case Qt::Key_Down:
            ptr = (wchar_t *) L"cursor-down";
            break;
        case Qt::Key_Left:
            ptr = (wchar_t *) L"cursor-left";
            break;
        case Qt::Key_Right:
            ptr = (wchar_t *) L"cursor-right";
            break;
        case Qt::Key_PageUp:
            ptr = (wchar_t *) L"page-up";
            break;
        case Qt::Key_PageDown:
            ptr = (wchar_t *) L"page-down";
            break;
        case Qt::Key_Home:
            ptr = (wchar_t *) L"home";
            break;
        case Qt::Key_End:
            ptr = (wchar_t *) L"end";
            break;
        case Qt::Key_Space:
            ptr = (wchar_t *) L"space";
            break;
        case Qt::Key_F1:
            ptr = (wchar_t *) L"f1";
            break;
        case Qt::Key_F2:
            ptr = (wchar_t *) L"f2";
            break;
        case Qt::Key_F3:
            ptr = (wchar_t *) L"f3";
            break;
        case Qt::Key_F4:
            ptr = (wchar_t *) L"f4";
            break;
        case Qt::Key_F5:
            ptr = (wchar_t *) L"f5";
            break;
        case Qt::Key_F6:
            ptr = (wchar_t *) L"f6";
            break;
        case Qt::Key_F7:
            ptr = (wchar_t *) L"f7";
            break;
        case Qt::Key_F8:
            ptr = (wchar_t *) L"f8";
            break;
        case Qt::Key_F9:
            ptr = (wchar_t *) L"f9";
            break;
        case Qt::Key_F10:
            ptr = (wchar_t *) L"f10";
            break;
        case Qt::Key_F11:
            ptr = (wchar_t *) L"f11";
            break;
        case Qt::Key_F12:
            ptr = (wchar_t *) L"f12";
            break;
        case Qt::Key_Insert:
            ptr = (wchar_t *) L"insert";
            break;
        case Qt::Key_Backspace:
            ptr = (wchar_t *) L"backspace";
            break;
        case Qt::Key_Delete:
            ptr = (wchar_t *) L"delete";
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            ptr = (wchar_t *) L"enter";
            break;
        case Qt::Key_Escape:
            ptr = (wchar_t *) L"escape";
            break;

        default:
            break;
        }
    }

    if (ptr == NULL)
        k = qstring_to_v(event->text());
    else
        k = MPDM_S(ptr);

    if (k != NULL)
        mp_process_event(k);

    if (mp_keypress_throttle(1))
        update();
}


void MPArea::mousePressEvent(QMouseEvent *event)
{
    wchar_t *ptr = NULL;

    mouse_down = 1;

    QPoint pos = event->pos();

    mpdm_set_wcs(MP, MPDM_I(pos.x() / font_width),  L"mouse_x");
    mpdm_set_wcs(MP, MPDM_I(pos.y() / font_height), L"mouse_y");

    switch (event->button()) {
    case Qt::LeftButton:
        ptr = (wchar_t *) L"mouse-left-button";
        break;
    case Qt::MidButton:
        ptr = (wchar_t *) L"mouse-middle-button";
        break;
    case Qt::RightButton:
        ptr = (wchar_t *) L"mouse-right-button";
        break;
    default:
        break;
    }

    if (ptr != NULL)
        mp_process_event(MPDM_S(ptr));

    update();
}


void MPArea::mouseDoubleClickEvent(QMouseEvent *event)
{
    wchar_t *ptr = NULL;

    mouse_down = 1;

    QPoint pos = event->pos();

    mpdm_set_wcs(MP, MPDM_I(pos.x() / font_width),  L"mouse_x");
    mpdm_set_wcs(MP, MPDM_I(pos.y() / font_height), L"mouse_y");

    switch (event->button()) {
    case Qt::LeftButton:
        ptr = (wchar_t *) L"mouse-left-dblclick";
    default:
        break;
    }

    if (ptr != NULL)
        mp_process_event(MPDM_S(ptr));

    update();
}


void MPArea::mouseReleaseEvent(QMouseEvent *event)
{
    mouse_down = 0;
}


void MPArea::mouseMoveEvent(QMouseEvent *event)
{
    if (mouse_down) {
        int x, y;

        QPoint pos = event->pos();

        /* mouse dragging */
        x = pos.x() / font_width;
        y = pos.y() / font_height;

        mpdm_set_wcs(MP, MPDM_I(x), L"mouse_to_x");
        mpdm_set_wcs(MP, MPDM_I(y), L"mouse_to_y");

        mp_process_event(MPDM_S(L"mouse-drag"));

        update();
    }
}


void MPArea::wheelEvent(QWheelEvent *event)
{
#ifdef CONFOPT_QT5
    if (event->angleDelta().y() > 0)
#else
    if (event->delta() > 0)
#endif
        mp_process_event(MPDM_S(L"mouse-wheel-up"));
    else
        mp_process_event(MPDM_S(L"mouse-wheel-down"));

    update();
}


void MPArea::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("text/uri-list"))
        event->acceptProposedAction();
}


void MPArea::dropEvent(QDropEvent *event)
{
    int n;
    mpdm_t v = qstring_to_v(event->mimeData()->text());
    mpdm_t l = MPDM_A(0);

    /* split the list of files */
    v = mpdm_split_wcs(v, L"\n");

    for (n = 0; n < (int) mpdm_size(v); n++) {
        wchar_t *ptr;
        mpdm_t w = mpdm_get_i(v, n);

        /* strip file:///, if found */
        ptr = mpdm_string(w);

        if (wcsncmp(ptr, L"file://", 7) == 0)
            ptr += 7;

        if (*ptr != L'\0')
            mpdm_push(l, MPDM_S(ptr));
    }

    mpdm_set_wcs(MP, l, L"dropped_files");

    event->acceptProposedAction();
    mp_process_event(MPDM_S(L"dropped-files"));

    update();
}


/** MPArea slots **/

void MPArea::from_scrollbar(int value)
{
    if (!ignore_scrollbar_signal) {
        mpdm_t v = mp_active();

        mp_set_y(v, value);

        /* set the vy to the same value */
        v = mpdm_get_wcs(v, L"txt");
        mpdm_set_wcs(v, MPDM_I(value), L"vy");

        update();
    }
}


void MPArea::from_filetabs(int value)
{
    if (value >= 0) {
        /* sets the active one */
        mpdm_set_wcs(MP, MPDM_I(value), L"active_i");
        update();
    }
}


void MPArea::from_menu(QAction * action)
{
    mp_process_action(qaction_to_action[action]);

    update();
}


void MPArea::from_timer(void)
{
    mp_process_event(MPDM_S(L"idle"));
    update();
}


/** driver functions **/

static mpdm_t qt4_drv_clip_to_sys(mpdm_t a, mpdm_t ctxt)
{
    mpdm_t v;

    QClipboard *qc = QApplication::clipboard();

    /* gets the clipboard and joins */
    v = mpdm_get_wcs(MP, L"clipboard");

    if (mpdm_size(v) != 0) {
        v = mpdm_ref(mpdm_join_wcs(v, L"\n"));
        qc->setText(v_to_qstring(v));
        mpdm_unref(v);
    }

    return NULL;
}


static mpdm_t qt4_drv_sys_to_clip(mpdm_t a, mpdm_t ctxt)
{
    QClipboard *qc = QApplication::clipboard();
    QString qs = qc->text();

    /* split and set as the clipboard */
    mpdm_set_wcs(MP, mpdm_split_wcs(qstring_to_v(qs), L"\n"), L"clipboard");

    return NULL;
}


static mpdm_t qt4_drv_shutdown(mpdm_t a, mpdm_t ctxt)
{
    mpdm_t v;

    if ((v = mpdm_get_wcs(MP, L"exit_message")) != NULL) {
        mpdm_write_wcs(stdout, mpdm_string(v));
        printf("\n");
    }

    return NULL;
}


#include "mpv_qk_common.moc"
