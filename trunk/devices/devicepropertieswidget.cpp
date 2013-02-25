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

#include "devicepropertieswidget.h"
#include "filenameschemedialog.h"
#include "covers.h"
#include "localize.h"
#include "icons.h"
#include "utils.h"
#include <QValidator>
#include <QTabWidget>

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

DevicePropertiesWidget::DevicePropertiesWidget(QWidget *parent)
    : QWidget(parent)
    , schemeDlg(0)
    , noCoverText(i18n("Don't copy covers"))
    , embedCoverText(i18n("Embed cover within each file"))
    , modified(false)
    , saveable(false)
{
    setupUi(this);
    configFilename->setIcon(Icons::configureIcon);
    coverMaxSize->insertItems(0, QStringList() << i18n("No maximum size") << i18n("400 pixels") << i18n("300 pixels") << i18n("200 pixels") << i18n("100 pixels"));
    fixVariousArtists->setToolTip(i18n("<p>When copying tracks to a device, and the 'Album Artist' is set to 'Various Artists', "
                                       "then Cantata will set the 'Artist' tag of all tracks to 'Various Artists' and the "
                                       "track 'Title' tag to 'TrackArtist - TrackTitle'.<hr/> When copying from a device, Cantata "
                                       "will check if 'Album Artist' and 'Artist' are both set to 'Various Artists'. If so, it "
                                       "will attempt to extract the real artist from the 'Title' tag, and remove the artist name "
                                       "from the 'Title' tag.</p>"));
    fixVariousArtistsLabel->setToolTip(fixVariousArtists->toolTip());

    useCache->setToolTip(i18n("<p>If you enable this, then Cantata will create a cache of the device's music library. "
                              "This will help to speed up subsequent library scans (as the cache file will be used instead of "
                              "having to read the tags of each file.)<hr/><b>NOTE:</b> If you use another application to update "
                              "the device's library, then this cache will become out-of-date. To rectify this, simply "
                              "click on the 'refresh' icon in the device list. This will cause the cache file to be removed, and "
                              "the contents of the device re-scanned.</p>"));
    useCacheLabel->setToolTip(useCache->toolTip());

    if (qobject_cast<QTabWidget *>(parent)) {
        verticalLayout->setMargin(4);
    }
}

#define REMOVE(w) \
    w->setVisible(false); \
    w->deleteLater(); \
    w=0;

void DevicePropertiesWidget::update(const QString &path, const DeviceOptions &opts, const QList<DeviceStorage> &storage, int props)
{
    bool allowCovers=(props&Prop_CoversAll)||(props&Prop_CoversBasic);
    albumCovers->clear();
    if (allowCovers) {
        if (props&Prop_CoversAll) {
            albumCovers->insertItems(0, QStringList() << noCoverText << embedCoverText << Covers::standardNames());
        } else {
            albumCovers->insertItems(0, QStringList() << noCoverText << embedCoverText);
        }
    }
    filenameScheme->setText(opts.scheme);
    vfatSafe->setChecked(opts.vfatSafe);
    asciiOnly->setChecked(opts.asciiOnly);
    ignoreThe->setChecked(opts.ignoreThe);
    replaceSpaces->setChecked(opts.replaceSpaces);
    origOpts=opts;

    if (props&Prop_Folder) {
        musicFolder->setText(path);
        connect(musicFolder, SIGNAL(textChanged(const QString &)), this, SLOT(checkSaveable()));
    } else {
        REMOVE(musicFolder);
        REMOVE(musicFolderLabel);
    }
    if (allowCovers) {
        albumCovers->setEditable(props&Prop_CoversAll);
        if (origOpts.coverName==Device::constNoCover) {
            origOpts.coverName=noCoverText;
            albumCovers->setCurrentIndex(0);
        }
        if (origOpts.coverName==Device::constEmbedCover) {
            origOpts.coverName=embedCoverText;
            albumCovers->setCurrentIndex(1);
        } else {
            albumCovers->setCurrentIndex(0);
            for (int i=1; i<albumCovers->count(); ++i) {
                if (albumCovers->itemText(i)==origOpts.coverName) {
                    albumCovers->setCurrentIndex(i);
                    break;
                }
            }
        }
        if (0!=origOpts.coverMaxSize) {
            int coverMax=origOpts.coverMaxSize/100;
            if (coverMax<0 || coverMax>=coverMaxSize->count()) {
                coverMax=0;
            }
            coverMaxSize->setCurrentIndex(0==coverMax ? 0 : (coverMaxSize->count()-coverMax));
        } else {
            coverMaxSize->setCurrentIndex(0);
        }
        albumCovers->setValidator(new CoverNameValidator(this));
        connect(albumCovers, SIGNAL(editTextChanged(const QString &)), this, SLOT(albumCoversChanged()));
        connect(coverMaxSize, SIGNAL(currentIndexChanged(int)), this, SLOT(checkSaveable()));
    } else {
        REMOVE(albumCovers);
        REMOVE(albumCoversLabel);
        REMOVE(coverMaxSize);
        REMOVE(coverMaxSizeLabel);
    }
    if (props&Prop_Va) {
        fixVariousArtists->setChecked(opts.fixVariousArtists);
        connect(fixVariousArtists, SIGNAL(stateChanged(int)), this, SLOT(checkSaveable()));
    } else {
        REMOVE(fixVariousArtists);
        REMOVE(fixVariousArtistsLabel);
    }
    if (props&Prop_Cache) {
        useCache->setChecked(opts.useCache);
        connect(useCache, SIGNAL(stateChanged(int)), this, SLOT(checkSaveable()));
    } else {
        REMOVE(useCache);
        REMOVE(useCacheLabel);
    }
    if (props&Prop_AutoScan) {
        autoScan->setChecked(opts.autoScan);
        connect(autoScan, SIGNAL(stateChanged(int)), this, SLOT(checkSaveable()));
    } else {
        REMOVE(autoScan);
        REMOVE(autoScanLabel);
    }

    if (props&Prop_Transcoder) {
        transcoderName->clear();
        transcoderName->addItem(i18n("Do not transcode"), QString());
        transcoderName->setCurrentIndex(0);
        transcoderValue->setVisible(false);
        transcoderWhenDifferentLabel->setVisible(false);
        transcoderWhenDifferent->setVisible(false);
        transcoderWhenDifferent->setChecked(opts.transcoderWhenDifferent);

        QList<Encoders::Encoder> encs=Encoders::getAvailable();

        if (encs.isEmpty()) {
            transcoderFrame->setVisible(false);
        } else {
            foreach (const Encoders::Encoder &e, encs) {
                transcoderName->addItem(i18n("Transcode to \"%1\"").arg(e.name), e.codec);
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
        connect(transcoderName, SIGNAL(currentIndexChanged(int)), this, SLOT(transcoderChanged()));
        connect(transcoderValue, SIGNAL(valueChanged(int)), this, SLOT(checkSaveable()));
    } else {
        REMOVE(transcoderFrame);
    }

    if (storage.count()<2) {
        REMOVE(defaultVolume);
        REMOVE(defaultVolumeLabel);
    } else {
        foreach (const DeviceStorage &ds, storage) {
            defaultVolume->addItem(i18nc("name (size free)", "%1 (%2 free)")
                                   .arg(ds.description).arg(Utils::formatByteSize(ds.size-ds.used)), ds.volumeIdentifier);
        }

        for (int i=0; i<defaultVolume->count(); ++i) {
            if (defaultVolume->itemData(i).toString()==opts.volumeId) {
                defaultVolume->setCurrentIndex(i);
                break;
            }
        }
        connect(defaultVolume,SIGNAL(currentIndexChanged(int)), this, SLOT(checkSaveable()));
    }

    origMusicFolder=path;
    connect(configFilename, SIGNAL(clicked()), SLOT(configureFilenameScheme()));
    connect(filenameScheme, SIGNAL(textChanged(const QString &)), this, SLOT(checkSaveable()));
    connect(vfatSafe, SIGNAL(stateChanged(int)), this, SLOT(checkSaveable()));
    connect(asciiOnly, SIGNAL(stateChanged(int)), this, SLOT(checkSaveable()));
    connect(ignoreThe, SIGNAL(stateChanged(int)), this, SLOT(checkSaveable()));
    connect(replaceSpaces, SIGNAL(stateChanged(int)), this, SLOT(checkSaveable()));

    if (albumCovers) {
        albumCoversChanged();
    }
}

void DevicePropertiesWidget::transcoderChanged()
{
    QString codec=transcoderName->itemData(transcoderName->currentIndex()).toString();
    if (codec.isEmpty()) {
        transcoderName->setToolTip(QString());
        transcoderValue->setVisible(false);
        transcoderWhenDifferentLabel->setVisible(false);
        transcoderWhenDifferent->setVisible(false);
    } else {
        Encoders::Encoder enc=Encoders::getEncoder(codec);
        transcoderName->setToolTip(enc.description);
        transcoderWhenDifferentLabel->setVisible(true);
        transcoderWhenDifferent->setVisible(true);
        if (enc.values.count()) {
            transcoderValue->setValues(enc);
            transcoderValue->setVisible(true);
        } else {
            transcoderValue->setVisible(false);
        }
    }
    checkSaveable();
}

void DevicePropertiesWidget::albumCoversChanged()
{
    if (coverMaxSize) {
        bool enableSize=albumCovers->currentText()!=noCoverText;
        coverMaxSize->setEnabled(enableSize);
        coverMaxSizeLabel->setEnabled(enableSize);
    }
    checkSaveable();
}

void DevicePropertiesWidget::checkSaveable()
{
    DeviceOptions opts=settings();
    bool checkFolder=musicFolder ? musicFolder->isVisible() : false;

    modified=opts!=origOpts;
    if (!modified && checkFolder) {
        modified=musicFolder->text().trimmed()!=origMusicFolder;
    }
    saveable=!opts.scheme.isEmpty() && (!checkFolder || !musicFolder->text().trimmed().isEmpty()) && !opts.coverName.isEmpty();
    if (saveable &&
        ( (-1!=opts.coverName.indexOf(noCoverText) && opts.coverName!=noCoverText) ||
          (-1!=opts.coverName.indexOf(embedCoverText) && opts.coverName!=embedCoverText) ) ) {
        saveable=false;
    }
    emit updated();
}

void DevicePropertiesWidget::configureFilenameScheme()
{
    if (!schemeDlg) {
        schemeDlg=new FilenameSchemeDialog(this);
        connect(schemeDlg, SIGNAL(scheme(const QString &)), filenameScheme, SLOT(setText(const QString &)));
    }
    schemeDlg->show(settings());
}

DeviceOptions DevicePropertiesWidget::settings()
{
    DeviceOptions opts;
    opts.scheme=filenameScheme->text().trimmed();
    opts.vfatSafe=vfatSafe->isChecked();
    opts.asciiOnly=asciiOnly->isChecked();
    opts.ignoreThe=ignoreThe->isChecked();
    opts.replaceSpaces=replaceSpaces->isChecked();
    opts.fixVariousArtists=fixVariousArtists->isChecked();
    opts.useCache=useCache ? useCache->isChecked() : false;
    opts.autoScan=autoScan ? autoScan->isChecked() : false;
    opts.transcoderCodec=QString();
    opts.transcoderValue=0;
    opts.transcoderWhenDifferent=false;
    opts.coverName=cover();
    opts.coverMaxSize=(!coverMaxSize || 0==coverMaxSize->currentIndex()) ? 0 : ((coverMaxSize->count()-coverMaxSize->currentIndex())*100);
    opts.volumeId=defaultVolume && defaultVolume->isVisible() ? defaultVolume->itemData(defaultVolume->currentIndex()).toString() : QString();
    if (transcoderFrame && transcoderFrame->isVisible()) {
        opts.transcoderCodec=transcoderName->itemData(transcoderName->currentIndex()).toString();

        if (!opts.transcoderCodec.isEmpty()) {
            Encoders::Encoder enc=Encoders::getEncoder(opts.transcoderCodec);

            opts.transcoderWhenDifferent=transcoderWhenDifferent->isChecked();
            if (!enc.isNull() && transcoderValue->value()<enc.values.count()) {
                opts.transcoderValue=enc.values.at(transcoderValue->value()).value;
            }
        }
    }
    return opts;
}

QString DevicePropertiesWidget::cover() const
{
    QString coverName=albumCovers ? albumCovers->currentText().trimmed() : QString();
    return coverName==noCoverText
            ? Device::constNoCover
            : coverName==embedCoverText
                ? Device::constEmbedCover
                : coverName;
}
