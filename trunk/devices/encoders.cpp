/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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
#include <KDE/KLocale>
#include <KDE/KStandardDirs>
#include <QtCore/QRegExp>
#include <QtCore/QProcess>

namespace Encoders
{

static QList<Encoder> installedEncoders;
static QString ffmpeg;

static void init()
{
    static bool initialised=false;
    if (!initialised) {
        initialised=true;

        ffmpeg=KStandardDirs::findExe("ffmpeg");

        if (ffmpeg.isEmpty()) {
            return;
        }
        QByteArray vbr = "~%1kb/s VBR";
        QByteArray cbr = "%1kb/s CBR";
        QByteArray quality = "%1 (~%2kb/s VBR)";
        QList<Encoder> initial;
        initial.append(
            Encoder(i18n("AAC (Non-Free)"),
                    i18nc("Feel free to redirect the english Wikipedia link to a local version, if "
                          "it exists.",
                          "<a href=http://en.wikipedia.org/wiki/Advanced_Audio_Coding>Advanced Audio "
                          "Coding</a> (AAC) is a patented lossy codec for digital audio.<br>AAC "
                          "generally achieves better sound quality than MP3 at similar bit rates. "
                           "It is a reasonable choice for the iPod and some other portable music "
                           "players. Non-Free implementation."),
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
                    QLatin1String("libfaac"),
                    QLatin1String("-aq"),
                    i18n("Expected average bitrate for variable bitrate encoding"),
                    QList<Setting>() << Setting(i18n(vbr, 25), 30)
                                     << Setting(i18n(vbr, 50), 55)
                                     << Setting(i18n(vbr, 70), 80)
                                     << Setting(i18n(vbr, 90), 105)
                                     << Setting(i18n(vbr, 120), 125)
                                     << Setting(i18n(vbr, 150), 155)
                                     << Setting(i18n(vbr, 170), 180)
                                     << Setting(i18n(vbr, 180), 205)
                                     << Setting(i18n(vbr, 190), 230)
                                     << Setting(i18n(vbr, 200), 255)
                                     << Setting(i18n(vbr, 210), 280),
                    i18n("Smaller file"),
                    i18n("Better sound quality"),
                    5));
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
                         "generates a comparably big file<br/>"
                         "On the other hand, a compression level of <b>8</b> makes compression quite slow but "
                         "produces the smallest file.<br/>"
                         "Note that since FLAC is by definition a lossless codec, the audio quality "
                         "of the output is exactly the same regardless of the compression level.<br/>"
                         "Also, levels above <b>5</b> dramatically increase compression time but create an only "
                         "slightly smaller file, and are not recommended."),
                    QLatin1String("flac"),
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
        initial.append(
            Encoder(i18n("MP3"),
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
                    QLatin1String("libmp3lame"),
                    QLatin1String("-aq"),
                    i18n("Expected average bitrate for variable bitrate encoding"),
                    QList<Setting>() << Setting(i18n(vbr, 80), 9)
                                     << Setting(i18n(vbr, 100), 8)
                                     << Setting(i18n(vbr, 120), 7)
                                     << Setting(i18n(vbr, 140), 6)
                                     << Setting(i18n(vbr, 160), 5)
                                     << Setting(i18n(vbr, 175), 4)
                                     << Setting(i18n(vbr, 190), 3)
                                     << Setting(i18n(vbr, 205), 2)
                                     << Setting(i18n(vbr, 220), 1)
                                     << Setting(i18n(vbr, 240), 0),
                    i18n("Smaller file"),
                    i18n("Better sound quality"),
                    4));
        initial.append(
            Encoder(i18n("Ogg Vorbis"),
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
                    QLatin1String("libvorbis"),
                    QLatin1String("-aq"),
                    i18n("Quality rating"),
                    QList<Setting>() << Setting(i18n(quality, -1, 45), -1)
                                     << Setting(i18n(quality, 0, 64), 0)
                                     << Setting(i18n(quality, 1, 80), 1)
                                     << Setting(i18n(quality, 2, 96), 2)
                                     << Setting(i18n(quality, 3, 112), 3)
                                     << Setting(i18n(quality, 4, 128), 4)
                                     << Setting(i18n(quality, 5, 160), 5)
                                     << Setting(i18n(quality, 6, 192), 6)
                                     << Setting(i18n(quality, 7, 224), 7)
                                     << Setting(i18n(quality, 8, 256), 8)
                                     << Setting(i18n(quality, 9, 320), 9)
                                     << Setting(i18n(quality, 10, 500), 10),
                    i18n("Smaller file"),
                    i18n("Better sound quality"),
                    6));
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
                    QLatin1String("wmav2"),
                    QLatin1String("-ab"),
                    i18n("Bitrate"),
                    QList<Setting>() << Setting(i18n(cbr, 64), 65*1000)
                                     << Setting(i18n(cbr, 80), 75*1000)
                                     << Setting(i18n(cbr, 96), 88*1000)
                                     << Setting(i18n(cbr, 112), 106*1000)
                                     << Setting(i18n(cbr, 136), 133*1000)
                                     << Setting(i18n(cbr, 182), 180*1000)
                                     << Setting(i18n(cbr, 275), 271*1000)
                                     << Setting(i18n(cbr, 550), 545*1000),
                    i18n("Smaller file"),
                    i18n("Better sound quality"),
                    4));

        QProcess proc;
        proc.start(ffmpeg, QStringList() << "-codecs");
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
        foreach (const Encoder &encoder, initial) {
            if (output.contains(QRegExp(QLatin1String(".EA... ")+encoder.codec))) {
                installedEncoders.append(encoder);
            }
        }
    }
}

QString Encoder::changeExtension(const QString &file)
{
    if (extension.isEmpty()) {
        return file;
    }

    QString f(file);
    int pos=f.lastIndexOf('.');
    if (pos>1) {
        f=f.left(pos+1);
    }
    return f+extension;
}

QStringList Encoder::params(int value, const QString &in, const QString &out)
{
    QStringList p;
    p << ffmpeg << "-i" << in << "-acodec" << codec;
    if (!ffmpegParam.isEmpty() && values.size()>1) {
        bool increase=values.at(0).value<values.at(1).value;
        int v=values.at(defaultValueIndex).value;
        foreach (const Setting &s, values) {
            if ((increase && s.value>=value) || (!increase && s.value<=value)) {
                break;
            } else {
                v=s.value;
            }
        }
        p << ffmpegParam << QString::number(v);
    }
    p << QLatin1String("-map_metadata") << QLatin1String("0:0") << out;
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
