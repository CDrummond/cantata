if [ "`uname`" != "Darwin" ] ; then
    echo "ERROR: Run under OSX"
    exit
fi

dir=cantata.iconset
mkdir $dir
for s in 16 32 128 256 512 ; do
    cp ../icons/cantata$s.png $dir/icon_"$s"x"$s".png
done

cp ../icons/cantata32.png $dir/icon_16x16@2x.png
cp ../icons/cantata64.png $dir/icon_32x32@2x.png
cp ../icons/cantata256.png $dir/icon_128x128@2x.png
cp ../icons/cantata512.png $dir/icon_256x256@2x.png
cp ../icons/cantata1024.png $dir/icon_512x512@2x.png

iconutil -c icns $dir
rm $dir/icon_*.png
rmdir $dir
