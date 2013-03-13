/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * Large portions of this file, mainly the textual descriptions, etc, are taken from
 * Amarok - hence the (c) notice below...
 */
/****************************************************************************************
 * Copyright (c) 2010 Téo Mrnjavac <teo@kde.org>                                        *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) any later           *
 * version.                                                                             *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Public License for more Encoder.             *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#include "encoders.h"
#include "utils.h"
#include "localize.h"
#include <QRegExp>
#include <QProcess>
#include <QDebug>

namespace Encoders
{

static QList<Encoder> installedEncoders;
static bool usingAvconv=false;

static void insertCodec(const QString &cmd, const QString &param, Encoder &enc)
{
    QString command=Utils::findExe(cmd);
    if (!command.isEmpty()) {
        int index=installedEncoders.indexOf(enc);
        enc.codec=cmd;
        enc.param=param;
        enc.transcoder=false;
        enc.app=command;
        if (-1!=index) {
            Encoder orig=installedEncoders.takeAt(index);
            orig.name+=QLatin1String(" (ffmpeg)");
            installedEncoders.insert(index, orig);
            enc.name+=QLatin1String(" (")+cmd+QChar(')');
            installedEncoders.insert(index+1, enc);
        } else {
            installedEncoders.append(enc);
        }
    }
}

static void init()
{
    static bool initialised=false;
    if (!initialised) {
        initialised=true;

        QString command=Utils::findExe(QLatin1String("ffmpeg"));
        if (command.isEmpty()) {
            command=Utils::findExe(QLatin1String("avconv"));
            usingAvconv=true;
        }

        QByteArray vbr = "~%1kb/s VBR";
        QByteArray cbr = "%1kb/s CBR";
        QByteArray quality = "%1 (~%2kb/s VBR)";

        Encoder aac(QLatin1String("AAC"),
                    i18nc("Feel free to redirect the english Wikipedia link to a local version, if "
                          "it exists.",
                          "<a href=http://en.wikipedia.org/wiki/Advanced_Audio_Coding>Advanced Audio "
                          "Coding</a> (AAC) is a patented lossy codec for digital audio.<br>AAC "
                          "generally achieves better sound quality than MP3 at similar bit rates. "
                          "It is a reasonable choice for the iPod and some other portable music "
                          "players."),
                    i18n("The bitrate is a measure of the quantity of data used to represent a second "
                         "of the audio track.<br>The <b>AAC</b> encoder used by Cantata supports a <a href="
                         "http://en.wikipedia.org/wiki/Variable_bitrate#Advantages_and_disadvantages_of_VBR"
                         ">variable bitrate (VBR)</a> setting, which means that the bitrate value "
                         "fluctuates along the track based on the complexity of the audio content. "
                         "More complex intervals of data are encoded with a higher bitrate than less "
                         "complex ones; this approach yields overall better quality and a smaller file "
                         "than having a constant bitrate throughout the track.<br>"
                         "For this reason, the bitrate measure in this slider is just an estimate "
                         "of the <a href=http://www.ffmpeg.org/faq.html#SEC21>average bitrate</a> of "
                         "the encoded track.<br>"
                         "<b>150kb/s</b> is a good choice for music listening on a portable player.<br/>"
                         "Anything below <b>120kb/s</b> might be unsatisfactory for music and anything above "
                         "<b>200kb/s</b> is probably overkill."),
                    QLatin1String("m4a"),
                    command,
                    QLatin1String("libfaac"),
                    QLatin1String("-aq"),
                    i18n("Expected average bitrate for variable bitrate encoding"),
                    QList<Setting>() << Setting(i18n(vbr).arg(25), 30)
                    << Setting(i18n(vbr).arg(50), 55)
                    << Setting(i18n(vbr).arg(70), 80)
                    << Setting(i18n(vbr).arg(90), 105)
                    << Setting(i18n(vbr).arg(120), 125)
                    << Setting(i18n(vbr).arg(150), 155)
                    << Setting(i18n(vbr).arg(170), 180)
                    << Setting(i18n(vbr).arg(180), 205)
                    << Setting(i18n(vbr).arg(190), 230)
                    << Setting(i18n(vbr).arg(200), 255)
                    << Setting(i18n(vbr).arg(210), 280),
                    i18n("Smaller file"),
                    i18n("Better sound quality"),
                    5);

        Encoder lame(QLatin1String("MP3"),
                    i18nc("Feel free to redirect the english Wikipedia link to a local version, if "
                          "it exists.",
                          "<a href=http://en.wikipedia.org/wiki/MP3>MPEG Audio Layer 3</a> (MP3) is "
                          "a patented digital audio codec using a form of lossy data compression."
                          "<br>In spite of its shortcomings, it is a common format for consumer "
                          "audio storage, and is widely supported on portable music players."),
                    i18n("The bitrate is a measure of the quantity of data used to represent a "
                         "second of the audio track.<br>The <b>MP3</b> encoder used by Cantata supports "
                         "a <a href=http://en.wikipedia.org/wiki/MP3#VBR>variable bitrate (VBR)</a> "
                         "setting, which means that the bitrate value fluctuates along the track "
                         "based on the complexity of the audio content. More complex intervals of "
                         "data are encoded with a higher bitrate than less complex ones; this "
                         "approach yields overall better quality and a smaller file than having a "
                         "constant bitrate throughout the track.<br>"
                         "For this reason, the bitrate measure in this slider is just an estimate "
                         "of the average bitrate of the encoded track.<br>"
                         "<b>160kb/s</b> is a good choice for music listening on a portable player.<br/>"
                         "Anything below <b>120kb/s</b> might be unsatisfactory for music and anything above "
                         "<b>205kb/s</b> is probably overkill."),
                    QLatin1String("mp3"),
                    command,
                    QLatin1String("libmp3lame"),
                    QLatin1String("-aq"),
                    i18n("Expected average bitrate for variable bitrate encoding"),
                    QList<Setting>() << Setting(i18n(vbr).arg(80), 9)
                                     << Setting(i18n(vbr).arg(100), 8)
                                     << Setting(i18n(vbr).arg(120), 7)
                                     << Setting(i18n(vbr).arg(140), 6)
                                     << Setting(i18n(vbr).arg(160), 5)
                                     << Setting(i18n(vbr).arg(175), 4)
                                     << Setting(i18n(vbr).arg(190), 3)
                                     << Setting(i18n(vbr).arg(205), 2)
                                     << Setting(i18n(vbr).arg(220), 1)
                                     << Setting(i18n(vbr).arg(240), 0),
                    i18n("Smaller file"),
                    i18n("Better sound quality"),
                    4);

    Encoder ogg(i18n("Ogg Vorbis"),
                   i18nc("Feel free to redirect the english Wikipedia link to a local version, if "
                         "it exists.",
                         "<a href=http://en.wikipedia.org/wiki/Vorbis>Ogg Vorbis</a> is an open "
                         "and royalty-free audio codec for lossy audio compression.<br>It produces "
                         "smaller files than MP3 at equivalent or higher quality. Ogg Vorbis is an "
                         "all-around excellent choice, especially for portable music players that "
                         "support it."),
                   i18n("The bitrate is a measure of the quantity of data used to represent a "
                        "second of the audio track.<br>The <b>Vorbis</b> encoder used by Cantata supports "
                        "a <a href=http://en.wikipedia.org/wiki/Vorbis#Technical_Encoder>variable bitrate "
                        "(VBR)</a> setting, which means that the bitrate value fluctuates along the track "
                        "based on the complexity of the audio content. More complex intervals of "
                        "data are encoded with a higher bitrate than less complex ones; this "
                        "approach yields overall better quality and a smaller file than having a "
                        "constant bitrate throughout the track.<br>"
                        "The Vorbis encoder uses a quality rating between -1 and 10 to define "
                        "a certain expected audio quality level. The bitrate measure in this slider is "
                        "just a rough estimate (provided by Vorbis) of the average bitrate of the encoded "
                        "track given a quality value. In fact, with newer and more efficient Vorbis versions the "
                        "actual bitrate is even lower.<br>"
                        "<b>5</b> is a good choice for music listening on a portable player.<br/>"
                        "Anything below <b>3</b> might be unsatisfactory for music and anything above "
                        "<b>8</b> is probably overkill."),
                   QLatin1String("ogg"),
                   command,
                   QLatin1String("libvorbis"),
                   QLatin1String("-aq"),
                   i18n("Quality rating"),
                   QList<Setting>() << Setting(i18n(quality).arg(-1).arg(45), -1)
                   << Setting(i18n(quality).arg(0).arg(64), 0)
                   << Setting(i18n(quality).arg(1).arg(80), 1)
                   << Setting(i18n(quality).arg(2).arg(96), 2)
                   << Setting(i18n(quality).arg(3).arg(112), 3)
                   << Setting(i18n(quality).arg(4).arg(128), 4)
                   << Setting(i18n(quality).arg(5).arg(160), 5)
                   << Setting(i18n(quality).arg(6).arg(192), 6)
                   << Setting(i18n(quality).arg(7).arg(224), 7)
                   << Setting(i18n(quality).arg(8).arg(256), 8)
                   << Setting(i18n(quality).arg(9).arg(320), 9)
                   << Setting(i18n(quality).arg(10).arg(500), 10),
                   i18n("Smaller file"),
                   i18n("Better sound quality"),
                   6);

        if (!command.isEmpty()) {
            QList<Encoder> initial;
            initial.append(aac);
            initial.append(
                        Encoder(i18n("Apple Lossless"),
                                i18nc("Feel free to redirect the english Wikipedia link to a local version, if "
                                      "it exists.",
                                      "<a href=http://en.wikipedia.org/wiki/Apple_Lossless>Apple Lossless</a> "
                                      "(ALAC) is an audio codec for lossless compression of digital music.<br>"
                                      "Recommended only for Apple music players and players that do not support "
                                      "FLAC."),
                                QString(),
                                QLatin1String("m4a"),
                                command,
                                QLatin1String("alac"),
                                QString(),
                                QString(),
                                QList<Setting>(),
                                QString(),
                                QString(),
                                0));
            initial.append(
                        Encoder(i18n("FLAC"),
                                i18nc("Feel free to redirect the english Wikipedia link to a local version, if "
                                      "it exists.",
                                      "<a href=http://en.wikipedia.org/wiki/Free_Lossless_Audio_Codec>Free "
                                      "Lossless Audio Codec</a> (FLAC) is an open and royalty-free codec for "
                                      "lossless compression of digital music.<br>If you wish to store your music "
                                      "without compromising on audio quality, FLAC is an excellent choice."),
                                i18n("The <a href=http://flac.sourceforge.net/documentation_tools_flac.html>"
                                     "compression level</a> is an integer value between 0 and 8 that represents "
                                     "the tradeoff between file size and compression speed while encoding with <b>FLAC</b>.<br/> "
                                     "Setting the compression level to <b>0</b> yields the shortest compression time but "
                                     "generates a comparably big file.<br/>"
                                     "On the other hand, a compression level of <b>8</b> makes compression quite slow but "
                                     "produces the smallest file.<br/>"
                                     "Note that since FLAC is by definition a lossless codec, the audio quality "
                                     "of the output is exactly the same regardless of the compression level.<br/>"
                                     "Also, levels above <b>5</b> dramatically increase compression time but create an only "
                                     "slightly smaller file, and are not recommended."),
                                QLatin1String("flac"),
                                command,
                                QLatin1String("flac"),
                                QLatin1String("-compression_level"),
                                i18n("Compression level"),
                                QList<Setting>() << Setting(QString::number(0), 0)
                                << Setting(QString::number(1), 1)
                                << Setting(QString::number(2), 2)
                                << Setting(QString::number(3), 3)
                                << Setting(QString::number(4), 4)
                                << Setting(QString::number(5), 5)
                                << Setting(QString::number(6), 6)
                                << Setting(QString::number(7), 7)
                                << Setting(QString::number(8), 8),
                                i18n("Faster compression"),
                                i18n("Smaller file"),
                                5));
            initial.append(lame);
            initial.append(ogg);
            initial.append(
                        Encoder(i18n("Windows Media Audio"),
                                i18nc("Feel free to redirect the english Wikipedia link to a local version, if "
                                      "it exists.",
                                      "<a href=http://en.wikipedia.org/wiki/Windows_Media_Audio>Windows Media "
                                      "Audio</a> (WMA) is a proprietary codec developed by Microsoft for lossy "
                                      "audio compression.<br>Recommended only for portable music players that "
                                      "do not support Ogg Vorbis."),
                                i18n("The bitrate is a measure of the quantity of data used to represent a "
                                     "second of the audio track.<br>Due to the limitations of the proprietary <b>WMA</b> "
                                     "format and the difficulty of reverse-engineering a proprietary encoder, the "
                                     "WMA encoder used by Cantata sets a <a href=http://en.wikipedia.org/wiki/"
                                     "Windows_Media_Audio#Windows_Media_Audio>constant bitrate (CBR)</a> setting.<br>"
                                     "For this reason, the bitrate measure in this slider is a pretty accurate estimate "
                                     "of the bitrate of the encoded track.<br>"
                                     "<b>136kb/s</b> is a good choice for music listening on a portable player.<br/>"
                                     "Anything below <b>112kb/s</b> might be unsatisfactory for music and anything above "
                                     "<b>182kb/s</b> is probably overkill."),
                                QLatin1String("wma"),
                                command,
                                QLatin1String("wmav2"),
                                QLatin1String("-ab"),
                                i18n("Bitrate"),
                                QList<Setting>() << Setting(i18n(cbr).arg(64), 65*1000)
                                << Setting(i18n(cbr).arg(80), 75*1000)
                                << Setting(i18n(cbr).arg(96), 88*1000)
                                << Setting(i18n(cbr).arg(112), 106*1000)
                                << Setting(i18n(cbr).arg(136), 133*1000)
                                << Setting(i18n(cbr).arg(182), 180*1000)
                                << Setting(i18n(cbr).arg(275), 271*1000)
                                << Setting(i18n(cbr).arg(550), 545*1000),
                                i18n("Smaller file"),
                                i18n("Better sound quality"),
                                4));

            QProcess proc;
            proc.start(command, QStringList() << "-codecs");
            if (proc.waitForStarted()) {
                proc.waitForFinished();
            }

            if (0!=proc.exitCode()) {
                return;
            }

            QString output=proc.readAllStandardOutput();
            if (output.simplified().isEmpty()) {
                return;
            }

            QStringList lines=output.split('\n', QString::SkipEmptyParts);
            foreach (const QString &line, lines) {
                int pos=line.indexOf(QRegExp(QLatin1String("[\\. D]EA")));
                if (0==pos || 1==pos) {
                    QList<Encoder>::Iterator it(initial.begin());
                    QList<Encoder>::Iterator end(initial.end());
                    for (; it!=end; ++it) {
                        if (line.contains((*it).codec)) {
                            installedEncoders.append((*it));
                            initial.erase(it);
                            break;
                        }
                    }
                    if (initial.isEmpty()) {
                        break;
                    }
                }
            }
        }

        insertCodec(QLatin1String("faac"), QLatin1String("-q"), aac);
        insertCodec(QLatin1String("lame"), QLatin1String("-V"), lame);
        insertCodec(QLatin1String("oggenc"), QLatin1String("-q"), ogg);
        qSort(installedEncoders);
    }
}

bool Encoder::operator<(const Encoder &o) const
{
    return name.compare(o.name)<0;
}

QString Encoder::changeExtension(const QString &file)
{
    return Utils::changeExtension(file, extension);
}

bool Encoder::isDifferent(const QString &file)
{
    return file!=changeExtension(file);
}

QStringList Encoder::params(int value, const QString &in, const QString &out) const
{
    QStringList p;
    if (transcoder) {
        p << app << QLatin1String("-i") << in << QLatin1String("-threads") << QLatin1String("0") << QLatin1String(usingAvconv ? "-c:a" : "-acodec") << codec;
    } else {
        p << app;
    }
    if (!param.isEmpty() && values.size()>1) {
        bool increase=values.at(0).value<values.at(1).value;
        int v=values.at(defaultValueIndex).value;
        foreach (const Setting &s, values) {
            if ((increase && s.value>value) || (!increase && s.value<value)) {
                break;
            } else {
                v=s.value;
            }
        }
        p << param << QString::number(v);
    }

    if (transcoder) {
        p << "-y"; // Overwrite destination
    } else {
        p << in;
    }
    p << out;
    return p;
}

QList<Encoder> getAvailable()
{
    init();
    return installedEncoders;
}

Encoder getEncoder(const QString &codec)
{
    init();
    foreach (const Encoder &encoder, installedEncoders) {
        if (encoder.codec==codec) {
            return encoder;
        }
    }
    return Encoder();
}

}
