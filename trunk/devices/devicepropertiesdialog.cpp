/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "devicepropertiesdialog.h"
#include "filenameschemedialog.h"
#include "covers.h"
#include <KDE/KGlobal>
#include <KDE/KLocale>
#include <KDE/KMessageBox>
#include <QtGui/QValidator>

class CoverNameValidator : public QValidator
{
    public:

    CoverNameValidator(QObject *parent) : QValidator(parent) { }

    State validate(QString &input, int &) const
    {
        int dotCount(0);

        for (int i=0; i<input.length(); ++i) {
            if (QChar('.')==input[i]) {
                if (++dotCount>1) {
                    return Invalid;
                }
            }
            else if (!input[i].isLetterOrNumber() || input[i].isSpace()) {
                return Invalid;
            }
        }
        if (input.endsWith('.')) {
            return Intermediate;
        }

        return Acceptable;
    }

//     void fixup(QString &input) const
//     {
//         QString out;
//         int dotCount(0);
//         for (int i=0; i<input.length(); ++i) {
//             if (input[i].isLetterOrNumber() && !input[i].isSpace()) {
//                 out+=input[i];
//             } else if (QChar('.')==input[i] && ++dotCount<1) {
//                 out+=input[i];
//             }
//         }
//
//         if (!out.endsWith(".jpg") && !out.endsWith(".png")) {
//             out.replace(".", "");
//             out+=".jpg";
//         }
//         input=out;
//     }
};

DevicePropertiesDialog::DevicePropertiesDialog(QWidget *parent)
    : KDialog(parent)
    , schemeDlg(0)
    , noCoverText(i18n("Don't copy covers"))
{
    setButtons(KDialog::Ok|KDialog::Cancel);
    setCaption(i18n("Device Properties"));
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowModality(Qt::WindowModal);
    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    setMainWidget(mainWidet);
    configFilename->setIcon(QIcon::fromTheme("configure"));
    albumCovers->insertItems(0, QStringList() << noCoverText << Covers::standardNames());
    fixVariousArtists->setToolTip(i18n("<p>When copying tracks to a device, and the 'Album Artist' is set to 'Various Artists', "
                                       "then Cantata will set the 'Artist' tag of all tracks to 'Various Artists' and the "
                                       "track 'Title' tag to 'TrackArtist - TrackTitle'.<hr/> When copying from a device, Cantata "
                                       "will check if 'Album Artist' and 'Artist' are both set to 'Various Artists'. If so, it "
                                       "will attempt to extract the real artist from the 'Title' tag, and remove the artist name "
                                       "from the 'Title' tag.</p>"));
    fixVariousArtistsLabel->setToolTip(fixVariousArtists->toolTip());
}

void DevicePropertiesDialog::show(const QString &path, const QString &coverName, const Device::Options &opts, int props)
{
    filenameScheme->setText(opts.scheme);
    vfatSafe->setChecked(opts.vfatSafe);
    asciiOnly->setChecked(opts.asciiOnly);
    ignoreThe->setChecked(opts.ignoreThe);
    replaceSpaces->setChecked(opts.replaceSpaces);
    musicFolder->setText(path);
    musicFolder->setVisible(props&Prop_Folder);
    musicFolderLabel->setVisible(props&Prop_Folder);
    albumCovers->setVisible(props&Prop_Covers);
    albumCoversLabel->setVisible(props&Prop_Covers);
    fixVariousArtists->setVisible(props&Prop_Va);
    fixVariousArtistsLabel->setVisible(props&Prop_Va);
    fixVariousArtists->setChecked(opts.fixVariousArtists);
    transcoderFrame->setVisible(props&Prop_Transcoder);
    transcoderName->clear();
    transcoderName->addItem(i18n("Do not transcode"), QString());
    transcoderName->setCurrentIndex(0);
    transcoderValue->setVisible(false);

    if (props&Prop_Transcoder) {
        QList<Encoders::Encoder> encs=Encoders::getAvailable();

        if (encs.isEmpty()) {
            transcoderFrame->setVisible(false);
        } else {
            foreach (const Encoders::Encoder &e, encs) {
                transcoderName->addItem(i18n("Transcode to \"%1\"", e.name), e.codec);
            }

            if (opts.transcoderCodec.isEmpty()) {
                transcoderChanged();
            } else {
                Encoders::Encoder enc=Encoders::getEncoder(opts.transcoderCodec);
                if (!enc.isNull()) {
                    for (int i=1; i<transcoderName->count(); ++i) {
                        if (transcoderName->itemData(i).toString()==opts.transcoderCodec) {
                            transcoderName->setCurrentIndex(i);
                            transcoderChanged();
                            transcoderValue->setValue(opts.transcoderValue);
                            break;
                        }
                    }
                }
            }
        }
    }
    albumCovers->setCurrentIndex(0);
    origCoverName=coverName;
    if (origCoverName==Device::constNoCover) {
        origCoverName=noCoverText;
    }
    for (int i=0; i<albumCovers->count(); ++i) {
        if (albumCovers->itemText(i)==origCoverName) {
            albumCovers->setCurrentIndex(i);
            break;
        }
    }
    origOpts=opts;
    origMusicFolder=path;
    albumCovers->setValidator(new CoverNameValidator(this));

    connect(configFilename, SIGNAL(clicked()), SLOT(configureFilenameScheme()));
    connect(filenameScheme, SIGNAL(textChanged(const QString &)), this, SLOT(enableOkButton()));
    connect(vfatSafe, SIGNAL(stateChanged(int)), this, SLOT(enableOkButton()));
    connect(asciiOnly, SIGNAL(stateChanged(int)), this, SLOT(enableOkButton()));
    connect(ignoreThe, SIGNAL(stateChanged(int)), this, SLOT(enableOkButton()));
    connect(musicFolder, SIGNAL(textChanged(const QString &)), this, SLOT(enableOkButton()));
    connect(albumCovers, SIGNAL(editTextChanged(const QString &)), this, SLOT(enableOkButton()));
    connect(fixVariousArtists, SIGNAL(stateChanged(int)), this, SLOT(enableOkButton()));
    connect(transcoderName, SIGNAL(currentIndexChanged(int)), this, SLOT(transcoderChanged()));
    connect(transcoderValue, SIGNAL(valueChanged(int)), this, SLOT(enableOkButton()));

    KDialog::show();
    enableButtonOk(false);
}

void DevicePropertiesDialog::transcoderChanged()
{
    QString codec=transcoderName->itemData(transcoderName->currentIndex()).toString();
    if (codec.isEmpty()) {
        transcoderName->setToolTip(QString());
        transcoderValue->setVisible(false);
    } else {
        Encoders::Encoder enc=Encoders::getEncoder(codec);
        transcoderName->setToolTip(enc.description);
        if (enc.values.count()) {
            transcoderValue->setValues(enc);
            transcoderValue->setVisible(true);
        } else {
            transcoderValue->setVisible(false);
        }
    }
    enableOkButton();
}

void DevicePropertiesDialog::enableOkButton()
{
    Device::Options opts=settings();
    enableButtonOk(!opts.scheme.isEmpty() && (musicFolder->text().trimmed()!=origMusicFolder || opts!=origOpts || albumCovers->currentText()!=origCoverName));
}

void DevicePropertiesDialog::slotButtonClicked(int button)
{
    switch (button) {
    case KDialog::Ok:
        emit updatedSettings(musicFolder->text().trimmed(), albumCovers->currentText()==noCoverText ? Device::constNoCover : albumCovers->currentText(), settings());
        break;
    case KDialog::Cancel:
        emit cancelled();
        reject();
        break;
    default:
        break;
    }

    if (KDialog::Ok==button) {
        accept();
    }

    KDialog::slotButtonClicked(button);
}

void DevicePropertiesDialog::configureFilenameScheme()
{
    if (!schemeDlg) {
        schemeDlg=new FilenameSchemeDialog(this);
        connect(schemeDlg, SIGNAL(scheme(const QString &)), filenameScheme, SLOT(setText(const QString &)));
    }
    schemeDlg->show(settings());
}

Device::Options DevicePropertiesDialog::settings()
{
    Device::Options opts;
    opts.scheme=filenameScheme->text().trimmed();
    opts.vfatSafe=vfatSafe->isChecked();
    opts.asciiOnly=asciiOnly->isChecked();
    opts.ignoreThe=ignoreThe->isChecked();
    opts.replaceSpaces=replaceSpaces->isChecked();
    opts.fixVariousArtists=fixVariousArtists->isChecked();
    opts.transcoderCodec=QString();
    opts.transcoderValue=0;
    if (transcoderFrame->isVisible()) {
        opts.transcoderCodec=transcoderName->itemData(transcoderName->currentIndex()).toString();

        if (!opts.transcoderCodec.isEmpty()) {
            Encoders::Encoder enc=Encoders::getEncoder(opts.transcoderCodec);

            if (!enc.isNull() && transcoderValue->value()<enc.values.count()) {
                opts.transcoderValue=enc.values.at(transcoderValue->value()).value;
            }
        }
    }
    return opts;
}
