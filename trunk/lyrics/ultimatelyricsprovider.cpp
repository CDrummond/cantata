/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/* This file is part of Clementine.
   Copyright 2010, David Sansome <me@davidsansome.com>

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

//#include "songinfotextview.h"
#include "ultimatelyricsprovider.h"
#include "networkaccessmanager.h"
//#include "core/network.h"
#include "song.h"
#include <QtNetwork/QNetworkReply>
#include <QtCore/QTextCodec>
//#include <boost/scoped_ptr.hpp>

const int UltimateLyricsProvider::kRedirectLimit = 5;


UltimateLyricsProvider::UltimateLyricsProvider()
  : relevance_(0),
    redirect_count_(0)
{
}

void UltimateLyricsProvider::FetchInfo(int id, const Song& metadata) {
  // Get the text codec
  const QTextCodec* codec = QTextCodec::codecForName(charset_.toAscii().constData());
  if (!codec) {
    //qWarning() << "Invalid codec" << charset_;
    //emit Finished(id);
    emit InfoReady(id, QString());
    return;
  }

    // strip "featuring <someone else>" from the song.artist
    QString artistFixed=metadata.artist;
    QStringList toStrip;
    toStrip << QLatin1String(" ft. ") << QLatin1String(" feat. ") << QLatin1String(" featuring ");
    foreach (const QString s, toStrip) {
        int strip = artistFixed.toLower().indexOf( " ft. ");
        if ( strip != -1 ) {
            artistFixed = artistFixed.mid( 0, strip );
        }
    }

  // Fill in fields in the URL
  QString url_text(url_);
  DoUrlReplace("{artist}", artistFixed.toLower(),      &url_text);
  DoUrlReplace("{album}",  metadata.album.toLower(),   &url_text);
  DoUrlReplace("{title}",  metadata.title.toLower(),   &url_text);
  DoUrlReplace("{Artist}", artistFixed,                &url_text);
  DoUrlReplace("{Album}",  metadata.album,             &url_text);
  DoUrlReplace("{Title}",  metadata.title,             &url_text);
  DoUrlReplace("{Title2}", TitleCase(metadata.title),  &url_text);
  DoUrlReplace("{a}",      FirstChar(artistFixed),     &url_text);

  QUrl url(url_text);

  // Fetch the URL, follow redirects
  redirect_count_ = 0;
  QNetworkReply* reply = NetworkAccessManager::self()->get(QNetworkRequest(url));
  requests_[reply] = id;
  connect(reply, SIGNAL(finished()), SLOT(LyricsFetched()));
}

void UltimateLyricsProvider::LyricsFetched() {
  QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
  if (!reply)
    return;

  int id = requests_.take(reply);
  reply->deleteLater();

  if (reply->error() != QNetworkReply::NoError) {
    //emit Finished(id);
    emit InfoReady(id, QString());
    return;
  }

  // Handle redirects
  QVariant redirect_target = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
  if (redirect_target.isValid()) {
    if (redirect_count_ >= kRedirectLimit) {
      //emit Finished(id);
      emit InfoReady(id, QString());
      return;
    }

    QUrl target = redirect_target.toUrl();
    if (target.scheme().isEmpty() || target.host().isEmpty()) {
      QString path = target.path();
      target = reply->url();
      target.setPath(path);
    }

    redirect_count_ ++;
    QNetworkReply* reply = NetworkAccessManager::self()->get(QNetworkRequest(target));
    requests_[reply] = id;
    connect(reply, SIGNAL(finished()), SLOT(LyricsFetched()));
    return;
  }

  const QTextCodec* codec = QTextCodec::codecForName(charset_.toAscii().constData());
  const QString original_content = codec->toUnicode(reply->readAll());

  // Check for invalid indicators
  foreach (const QString& indicator, invalid_indicators_) {
    if (original_content.contains(indicator)) {
      //emit Finished(id);
       emit InfoReady(id, QString());
      return;
    }
  }

  QString lyrics;

  // Apply extract rules
  foreach (const Rule& rule, extract_rules_) {
    QString content = original_content;
    ApplyExtractRule(rule, &content);

    if (!content.isEmpty())
      lyrics = content;
  }

  // Apply exclude rules
  foreach (const Rule& rule, exclude_rules_) {
    ApplyExcludeRule(rule, &lyrics);
  }

#if 0
  if (!lyrics.isEmpty()) {
    CollapsibleInfoPane::Data data;
    data.id_ = "ultimatelyrics/" + name_;
    data.title_ = tr("Lyrics from %1").arg(name_);
    data.type_ = CollapsibleInfoPane::Data::Type_Lyrics;
    data.relevance_ = relevance();

    SongInfoTextView* editor = new SongInfoTextView;
    editor->SetHtml(lyrics);
    data.contents_ = editor;

    emit InfoReady(id, data);
  }
  emit Finished(id);
#endif

  lyrics=lyrics.trimmed();
  emit InfoReady(id, lyrics);
}

void UltimateLyricsProvider::ApplyExtractRule(const Rule& rule, QString* content) const {
  foreach (const RuleItem& item, rule) {
    if (item.second.isNull()) {
      *content = ExtractXmlTag(*content, item.first);
    } else {
      *content = Extract(*content, item.first, item.second);
    }
  }
}

QString UltimateLyricsProvider::ExtractXmlTag(const QString& source, const QString& tag) {
  QRegExp re("<(\\w+).*>"); // ಠ_ಠ
  if (re.indexIn(tag) == -1)
    return QString();

  return Extract(source, tag, "</" + re.cap(1) + ">");
}

QString UltimateLyricsProvider::Extract(const QString& source, const QString& begin, const QString& end) {
  int begin_idx = source.indexOf(begin);
  if (begin_idx == -1)
    return QString();
  begin_idx += begin.length();

  int end_idx = source.indexOf(end, begin_idx);
  if (end_idx == -1)
    return QString();

  return source.mid(begin_idx, end_idx - begin_idx - 1);
}

void UltimateLyricsProvider::ApplyExcludeRule(const Rule& rule, QString* content) const {
  foreach (const RuleItem& item, rule) {
    if (item.second.isNull()) {
      *content = ExcludeXmlTag(*content, item.first);
    } else {
      *content = Exclude(*content, item.first, item.second);
    }
  }
}

QString UltimateLyricsProvider::ExcludeXmlTag(const QString& source, const QString& tag) {
  QRegExp re("<(\\w+).*>"); // ಠ_ಠ
  if (re.indexIn(tag) == -1)
    return source;

  return Exclude(source, tag, "</" + re.cap(1) + ">");
}

QString UltimateLyricsProvider::Exclude(const QString& source, const QString& begin, const QString& end) {
  int begin_idx = source.indexOf(begin);
  if (begin_idx == -1)
    return source;

  int end_idx = source.indexOf(end, begin_idx + begin.length());
  if (end_idx == -1)
    return source;

  return source.left(begin_idx) + source.right(source.length() - end_idx - end.length());
}

QString UltimateLyricsProvider::FirstChar(const QString& text) {
  if (text.isEmpty())
    return QString();
  return text[0].toLower();
}

QString UltimateLyricsProvider::TitleCase(const QString& text) {
  if (text.length() == 0)
    return QString();
  if (text.length() == 1)
    return text[0].toUpper();
  return text[0].toUpper() + text.right(text.length() - 1).toLower();
}

void UltimateLyricsProvider::DoUrlReplace(const QString& tag, const QString& value,
                               QString* url) const {
  if (!url->contains(tag))
    return;

  // Apply URL character replacement
  QString value_copy(value);
  foreach (const UrlFormat& format, url_formats_) {
    QRegExp re("[" + QRegExp::escape(format.first) + "]");
    value_copy.replace(re, format.second);
  }

  url->replace(tag, value_copy, Qt::CaseInsensitive);
}
