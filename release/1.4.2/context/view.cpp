/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
 
#include "view.h"
#include "support/spinner.h"
#include "network/networkaccessmanager.h"
#include "gui/settings.h"
#include "widgets/textbrowser.h"
#include "support/gtkstyle.h"
#include "support/actioncollection.h"
#include "support/action.h"
#include "widgets/icons.h"
#include "support/localize.h"
#include <QLabel>
#include <QScrollBar>
#include <QImage>
#include <QGridLayout>
#include <QNetworkReply>
#include <QLocale>
#include <QBuffer>
#include <QFile>
#include <QStackedWidget>
#include <QComboBox>
#include <QMenu>
#include <QMouseEvent>
#include <QWheelEvent>

// Uncomment this #define to have header labels an images centered. Thsi is disabled, as the image
// centering only works if there is some text larger than 1 line to be displayed underneath :-(
//#define CONTEXT_CENTERED

static QString headerTag;
QString View::subTag;

QString View::encode(const QImage &img)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    #ifdef CONTEXT_CENTERED
    return QString("<tr><td align=\"center\"><img src=\"data:image/png;base64,%1\"/></td></tr>").arg(QString(buffer.data().toBase64()));
    #else
    return QString("<img src=\"data:image/png;base64,%1\"/>").arg(QString(buffer.data().toBase64()));
    #endif
}

void View::initHeaderTags()
{
    bool small=Settings::self()->wikipediaIntroOnly() ;
    headerTag=small ? "h2" : "h1";
    subTag=small ? "h3" : "h2";
}

static TextBrowser * createView(QWidget *parent)
{
    TextBrowser *text=new TextBrowser(parent);
    if (GtkStyle::isActive()) {
        text->verticalScrollBar()->setAttribute(Qt::WA_OpaquePaintEvent, false);
    }
    text->setOpenLinks(false);
    text->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    text->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
    text->setFrameShape(QFrame::NoFrame);
    text->setReadOnly(true);
    text->viewport()->setAutoFillBackground(false);
    return text;
}

ViewTextSelector::ViewTextSelector(QWidget *p)
    : QLabel(p)
    , current(0)
{
    setAttribute(Qt::WA_Hover, true);
    menu=new QMenu(this);
    setStyleSheet(QLatin1String("QLabel:hover {color:palette(highlight);}"));
    setMargin(Utils::scaleForDpi(2));
}

static QString viewText(const QString &s)
{
    // 0x25BE is unicode for small down arrow...
    return QLatin1String("<b>")+s+QLatin1String("  ")+QChar(0x25BE)+QLatin1String("</b>");
}

void ViewTextSelector::addItem(const QString &t)
{
    menu->addAction(t, this, SLOT(itemSelected()))->setData(items.count());
    if (text().isEmpty()) {
        setText(viewText(t));
        current=items.count();
    }
    items.append(t);
}

void ViewTextSelector::itemSelected()
{
    QAction *act=qobject_cast<QAction *>(sender());
    if (act) {
        setCurrentIndex(act->data().toInt());
    }
}

bool ViewTextSelector::event(QEvent *e)
{
    switch (e->type()) {
    case QEvent::MouseButtonPress:
        if (Qt::NoModifier==static_cast<QMouseEvent *>(e)->modifiers() && Qt::LeftButton==static_cast<QMouseEvent *>(e)->button()) {
            menu->exec(mapToGlobal(static_cast<QMouseEvent *>(e)->pos()));
        }
    case QEvent::Wheel: {
        int numDegrees = static_cast<QWheelEvent *>(e)->delta() / 8;
        int numSteps = numDegrees / 15;
        int newIndex = current;
        if (numSteps > 0) {
            for (int i = 0; i < numSteps; ++i) {
                newIndex++;
                if (newIndex>=items.count()) {
                    newIndex=0;
                }
            }
        } else {
            for (int i = 0; i > numSteps; --i) {
                newIndex--;
                if (newIndex<0) {
                    newIndex=items.count()-1;
                }
            }
        }
        setCurrentIndex(newIndex);
        break;
    }
    default:
        break;
    }
    return QLabel::event(e);
}

void ViewTextSelector::setCurrentIndex(int v)
{
    if (v<0 || v>=items.count() || v==current) {
        return;
    }
    current=v;
    setText(viewText(items.at(current)));
    emit activated(current);
}

View::View(QWidget *parent, const QStringList &views)
    : QWidget(parent)
    , needToUpdate(false)
    , spinner(0)
    , selector(0)
    , stack(0)
{
    QGridLayout *layout=new QGridLayout(this);
    header=new QLabel(this);

    if (views.isEmpty()) {
        TextBrowser *t=createView(this);
        texts.append(t);
    } else {
        stack=new QStackedWidget(this);
        selector=new ViewTextSelector(this);
        foreach (const QString &v, views) {
            TextBrowser *t=createView(stack);
            selector->addItem(v);
            stack->addWidget(t);
            texts.append(t);
        }
        connect(selector, SIGNAL(activated(int)), stack, SLOT(setCurrentIndex(int)));
        connect(selector, SIGNAL(activated(int)), this, SIGNAL(viewChanged()));
    }

    layout->setMargin(0);
    header->setWordWrap(true);
    header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    int row=0;
    layout->addItem(new QSpacerItem(1, layout->spacing(), QSizePolicy::Fixed, QSizePolicy::Fixed), row++, 0);
    layout->addWidget(header, row++, 0, 1, 2);
    if (views.isEmpty()) {
        layout->addWidget(texts.at(0), row++, 0, 1, 2);
    } else {
        layout->addWidget(selector, row, 0);
        layout->addItem(new QSpacerItem(1, 1, QSizePolicy::MinimumExpanding, QSizePolicy::Fixed), row++, 1);
        layout->addWidget(stack, row++, 0, 1, 2);
    }
    layout->addItem(new QSpacerItem(1, fontMetrics().height()/4, QSizePolicy::Fixed, QSizePolicy::Fixed), row++, 0);
    if (headerTag.isEmpty()) {
        initHeaderTags();
    }

    cancelJobAction=new Action(Icons::self()->cancelIcon, i18n("Cancel"), this);
    cancelJobAction->setEnabled(false);
    connect(cancelJobAction, SIGNAL(triggered()), SLOT(abort()));
    text=texts.at(0);
}

View::~View()
{
    abort();
}

void View::clear()
{
    setHeader(stdHeader);
    foreach (TextBrowser *t, texts) {
        t->clear();
    }
}

void View::setHeader(const QString &str)
{
    #ifdef CONTEXT_CENTERED
    header->setText("<"+headerTag+" align=\"center\">"+str+"</"+headerTag+">");
    #else
    header->setText("<"+headerTag+">"+str+"</"+headerTag+">");
    #endif
}

void View::setPicSize(const QSize &sz)
{
    foreach (TextBrowser *t, texts) {
        t->setPicSize(sz);
    }
}

QSize View::picSize() const
{
    return text->picSize();
}

QString View::createPicTag(const QImage &img, const QString &file)
{
    if (!file.isEmpty() && QFile::exists(file)) {
        #ifdef CONTEXT_CENTERED
        return QString("<tr><td align=\"center\"><img src=\"%1\"/></td></tr>").arg(file);
        #else
        return QString("<img src=\"%1\"/>").arg(file);
        #endif
    }
    if (img.isNull()) {
        return QString();
    }
    // No filename given, or file does not exist - therefore encode & scale image.
    return encode(img.scaled(texts.at(0)->picSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void View::showEvent(QShowEvent *e)
{
    if (needToUpdate) {
        update(currentSong, true);
    }
    needToUpdate=false;
    QWidget::showEvent(e);
}

void View::showSpinner(bool enableCancel)
{
    if (!spinner) {
        spinner=new Spinner(this);
        spinner->setWidget(this);
    }
    if (!spinner->isActive()) {
        spinner->start();
        if (enableCancel) {
            cancelJobAction->setEnabled(true);
        }
    }
}

void View::hideSpinner(bool disableCancel)
{
    if (spinner) {
        spinner->stop();
        if (disableCancel) {
            cancelJobAction->setEnabled(false);
        }
    }
}

void View::setEditable(bool e, int index)
{
    TextBrowser *t=texts.at(index);
    t->setReadOnly(!e);
    t->viewport()->setAutoFillBackground(e);
}

void View::setPal(const QPalette &pal, const QColor &linkColor, const QColor &prevLinkColor)
{
    foreach (TextBrowser *t, texts) {
        // QTextBrowser seems to save link colour within the HTML, so we need to manually
        // update this when the palette changes!
        QString old=t->toHtml();
        t->setPal(pal);
        old=old.replace("color:"+prevLinkColor.name()+";", "color:"+linkColor.name()+";");
        t->setHtml(old);
    }
    // header uses window/button text - so need to set these now...
    QPalette hdrPal=pal;
    hdrPal.setColor(QPalette::WindowText, pal.color(QPalette::Text));
    hdrPal.setColor(QPalette::ButtonText, pal.color(QPalette::Text));
    header->setPalette(hdrPal);
    if (selector) {
        selector->setPalette(hdrPal);
    }
}

void View::addEventFilter(QObject *obj)
{
    installEventFilter(obj);
    foreach (TextBrowser *t, texts) {
        t->installEventFilter(obj);
        t->viewport()->installEventFilter(obj);
    }
    header->installEventFilter(obj);
}

void View::setZoom(int z)
{
    foreach (TextBrowser *t, texts) {
        t->setZoom(z);
    }
    QFont f=header->font();
    f.setPointSize(f.pointSize()+z);
    header->setFont(f);

    if (selector) {
        QFont f=selector->font();
        f.setPointSize(f.pointSize()+z);
        selector->setFont(f);
    }
}

int View::getZoom()
{
    return text->zoom();
}

void View::setHtml(const QString &h, int index)
{
    texts.at(index)->setText(QLatin1String("<html><head><style type=text/css>a:link {color:")+text->palette().link().color().name()+
                              QLatin1String("; text-decoration:underline;}</style></head><body>")+h+QLatin1String("</body></html>"));
}

void View::abort()
{
}
