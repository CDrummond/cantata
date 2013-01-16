#!/bin/sh

for app in extractrc xgettext msgmerge lupdate lconvert msguniq sed grep ; do
    which $app > /dev/null 2>&1
    if [ $? -ne 0 ] ; then
        echo "ERROR: Could not find $app"
        exit
    fi
done

BASEDIR=".." # root of translatable sources
PROJECT="cantata" # project name
BUGADDR="" # MSGID-Bugs
WDIR=`pwd` # working dir
echo "Preparing rc files"
cd ${BASEDIR}
# we use simple sorting to make sure the lines do not jump around too much from system to system
find . -name '*.rc' -o -name '*.ui' -o -name '*.kcfg' | sort > ${WDIR}/rcfiles.list
xargs --arg-file=${WDIR}/rcfiles.list extractrc > ${WDIR}/rc.cpp
# additional string for KAboutData
echo 'i18nc("NAME OF TRANSLATORS","Your names");' >> ${WDIR}/rc.cpp
echo 'i18nc("EMAIL OF TRANSLATORS","Your emails");' >> ${WDIR}/rc.cpp
cd ${WDIR}
echo "Done preparing rc files"

echo "Extracting messages"
cd ${BASEDIR}
# see above on sorting
find . -name '*.cpp' -o -name '*.h' -o -name '*.c' | grep -v "solid-lite" | sort > ${WDIR}/infiles.list
echo "rc.cpp" >> ${WDIR}/infiles.list

cd ${WDIR}
xgettext --from-code=UTF-8 -C -kde -ci18n -ki18n:1 -ki18nc:1c,2 -ki18np:1,2 -ki18ncp:1c,2,3 -ktr2i18n:1 \
         -kI18N_NOOP:1 -kI18N_NOOP2:1c,2 -kaliasLocale -kki18n:1 -kki18nc:1c,2 -kki18np:1,2 -kki18ncp:1c,2,3 \
         --msgid-bugs-address="${BUGADDR}" \
         --files-from=infiles.list -D ${BASEDIR} -D ${WDIR} -o ${PROJECT}_kde.pot || { echo "error while calling xgettext. aborting."; exit 1; }

find .. -name '*.cpp' | xargs grep -l 'QObject::tr(' |  grep -v "solid-lite" | sort > ${WDIR}/infiles.qt.list
echo "../qtplural.h" >> ${WDIR}/infiles.qt.list

# If we just call lconvert this adds a msgctxt of "QObject|<any other context>" This seems to mess things up.
# So, pipe the output of lconvert into sed to change "QObject|<any other context>" to "<any other context>", and then into
# grep -Ev to remove any blank contexts...
lupdate @infiles.qt.list -ts ${PROJECT}.ts && lconvert -i ${PROJECT}.ts -of po | sed s^QObject\|^^g | grep -Ev 'msgctxt ""' > ${PROJECT}_qt.po && msguniq -o ${PROJECT}_qt.pot ${PROJECT}_qt.po && rm ${PROJECT}.ts ${PROJECT}_qt.po

Fix charset...
cat ${PROJECT}_kde.pot | sed s^charset=CHARSET^charset=UTF-8^g > ${PROJECT}_kde.pot-new
tail -n +7 ${PROJECT}_qt.pot | sed s^charset=CHARSET^charset=UTF-8^g > ${PROJECT}_qt.pot-new
cat ${PROJECT}_kde.pot-new > ${PROJECT}.pot
echo " " >> ${PROJECT}.pot
cat ${PROJECT}_qt.pot-new >> ${PROJECT}.pot

echo "Done extracting messages"

echo "Merging translations"
catalogs=`find . -name '*.po'`
for cat in $catalogs; do
    echo $cat
    msgmerge -o $cat.new $cat ${PROJECT}.pot
    mv $cat.new $cat
done
echo "Done merging translations"

echo "Cleaning up"
cd ${WDIR}
rm rcfiles.list
rm infiles.list
rm infiles.qt.list
rm ${PROJECT}_kde.pot ${PROJECT}_qt.pot ${PROJECT}_kde.pot-new ${PROJECT}_qt.pot-new
rm rc.cpp
echo "Done"
