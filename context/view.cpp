/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "spinner.h"
#include "networkaccessmanager.h"
#include "settings.h"
#include "textbrowser.h"
#include "gtkstyle.h"
#include <QLabel>
#include <QScrollBar>
#include <QImage>
#include <QBoxLayout>
#include <QNetworkReply>
#include <QLocale>
#include <QBuffer>
#include <QFile>

static QString headerTag;
QString View::subTag;

static QString encode(const QImage &img)
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    return QString("<img src=\"data:image/png;base64,%1\"><br><br>").arg(QString(buffer.data().toBase64()));
}

void View::initHeaderTags()
{
    bool small=Settings::self()->wikipediaIntroOnly() ;
    headerTag=small ? "h2" : "h1";
    subTag=small ? "h3" : "h2";
}

View::View(QWidget *parent)
    : QWidget(parent)
    , needToUpdate(false)
    , spinner(0)
{
    QVBoxLayout *layout=new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    header=new QLabel(this);
    text=new TextBrowser(this);

    layout->setMargin(0);
    header->setWordWrap(true);
    header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    text->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);

    layout->addItem(new QSpacerItem(1, layout->spacing(), QSizePolicy::Fixed, QSizePolicy::Fixed));
    layout->addWidget(header);
    layout->addWidget(text);
    layout->addItem(new QSpacerItem(1, fontMetrics().height()/4, QSizePolicy::Fixed, QSizePolicy::Fixed));
    text->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    if (GtkStyle::isActive()) {
        text->verticalScrollBar()->setAttribute(Qt::WA_OpaquePaintEvent, false);
    }
    text->setOpenLinks(false);
    setEditable(false);
    if (headerTag.isEmpty()) {
        initHeaderTags();
    }
}

void View::clear()
{
    setHeader(stdHeader);
    text->clear();
}

void View::setHeader(const QString &str)
{
    header->setText("<"+headerTag+">"+str+"</"+headerTag+">");
}

void View::setPicSize(const QSize &sz)
{
    text->setPicSize(sz);
}

QSize View::picSize() const
{
    return text->picSize();
}

QString View::createPicTag(const QImage &img, const QString &file)
{
    if (!file.isEmpty() && QFile::exists(file)) {
        return QString("<img src=\"%1\"><br>").arg(file);
    }
    if (img.isNull()) {
        return QString();
    }
    // No filename given, or file does not exist - therefore encode & scale image.
    return encode(img.scaled(text->picSize(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void View::showEvent(QShowEvent *e)
{
    if (needToUpdate) {
        update(currentSong, true);
    }
    needToUpdate=false;
    QWidget::showEvent(e);
}

void View::showSpinner()
{
    if (!spinner) {
        spinner=new Spinner(this);
        spinner->setWidget(this);
    }
    if (!spinner->isActive()) {
        spinner->start();
    }
}

void View::hideSpinner()
{
    if (spinner) {
        spinner->stop();
    }
}

void View::setEditable(bool e)
{
    text->setReadOnly(!e);
    text->setFrameShape(e ? QFrame::StyledPanel : QFrame::NoFrame);
    text->viewport()->setAutoFillBackground(e);
}

void View::setPal(const QPalette &pal, const QColor &linkColor, const QColor &prevLinkColor)
{
    // QTextBrowser seems to save link colour within the HTML, so we need to manually
    // update this when the palette changes!
    QString old=text->toHtml();
    text->setPal(pal);
    // header uses window/button text - so need to set these now...
    QPalette hdrPal=pal;
    hdrPal.setColor(QPalette::WindowText, pal.color(QPalette::Text));
    hdrPal.setColor(QPalette::ButtonText, pal.color(QPalette::Text));
    header->setPalette(hdrPal);
    old=old.replace("color:"+prevLinkColor.name()+";", "color:"+linkColor.name()+";");
    text->setHtml(old);
}

void View::addEventFilter(QObject *obj)
{
    installEventFilter(obj);
    text->installEventFilter(obj);
    text->viewport()->installEventFilter(obj);
    header->installEventFilter(obj);
}

void View::setZoom(int z)
{
    text->setZoom(z);
    QFont f=header->font();
    f.setPointSize(f.pointSize()+z);
    header->setFont(f);
}

int View::getZoom()
{
    return text->zoom();
}

void View::setHtml(const QString &h)
{
    text->setText(QLatin1String("<html><head><style type=text/css>a:link {color:")+text->palette().link().color().name()+
                  QLatin1String("; text-decoration:underline;}</style></head><body>")+h+QLatin1String("</body></html>"));
}

void View::searchResponse(const QString &r, const QString &l)
{
    Q_UNUSED(l)
    text->setText(r);
}
