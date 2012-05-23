#!/bin/bash
IFS="
"

if [ $# -ne 1 ] ; then
    echo "#############################################################################"
    echo "Simple script to covert an Amarok radio script into a Cantata XML radio file."
    echo ""
    echo "This script will read \"script.spec\" to scertain the amarok script name, and"
    echo "this will be used as the Cantata streams category, and the name of the Cantata"
    echo "XML file"
    echo ""
    echo "(C) 2012 Craig Drummond <craig.p.drummond@gmail.com>"
    echo "Released under GPL v2 or later"
    echo "#############################################################################"
    echo ""
    echo ""
    echo "Usage: $0 <amarok script folder>"
    echo ""
    echo " e.g.: $0 \"`kde4-config --localprefix`share/apps/amarok/uk_radio\""
    echo "       - this will create \"UK Radio.cantata\""
    echo ""
    exit 1
fi

if [ ! -r "$1/main.js" ] ; then
    echo "ERROR: \"$1/main.js\" does not exist / is not readable"
    exit 2
fi

if [ ! -r "$1/script.spec" ] ; then
    echo "ERROR: \"$1/script.spec\" does not exist / is not readable"
    exit 2
fi

scriptName=`cat "$1/script.spec" | grep -v "X-KDE" | grep "^Name=" | awk -F= '{print $2}' | sed s^'&'^'&amp;'^g | sed s^\"^"&quot;"^g | sed s^\<^"&lt;"^g | sed s^\>^"&gt;"^g`
if [ "$scriptName" = "" ] ; then
    scriptName="Amarok Radio"
fi

cantataFile=$scriptName.cantata

if [ -f "$cantataFile" ] ; then
    echo "ERROR: \"$cantataFile\" already exists, please remove this first"
    exit
fi

found=0
for line in `cat "$1/main.js" | grep Station` ; do
    name=`echo "$line" | awk -F\" '{print $2}' | sed s^'&'^'&amp;'^g | sed s^\"^"&quot;"^g | sed s^\<^"&lt;"^g | sed s^\>^"&gt;"^g`
    url=`echo "$line" | awk -F\" '{print $4}' | sed s^'&'^'&amp;'^g | sed s^\"^"&quot;"^g | sed s^\<^"&lt;"^g | sed s^\>^"&gt;"^g`
    if [ "$name" != "" ] && [ "$url" != "" ] ; then
        if [ $found -eq 0 ] ; then
            echo "<cantata version=\"1.0\">" > "$cantataFile"
            echo " <category name=\""$scriptName"\">" >> "$cantataFile"
        fi
        echo '  <stream name="'$name'" url="'$url'"/>' >> "$cantataFile"
        let "found=found +1"
    fi
done

if [ $found -gt 0 ] ; then
    echo " </category>" >> "$cantataFile"
    echo "</cantata>" >> "$cantataFile"
    echo "Created \"$cantataFile\" with $found streams"
fi
