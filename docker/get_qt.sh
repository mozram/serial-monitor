#!/bin/sh -xe
# Script to install Qt 6 in docker container

[ "$AQT_VERSION" ] || AQT_VERSION=3.3.0
[ "$AQT_URL" ] || AQT_URL="https://github.com/miurahr/aqtinstall/releases/download/v$AQT_VERSION/aqt.exe"
[ "$AQT_SHA256" ] || AQT_SHA256=4f74d4c95c464d238d7e17ec2d9b7f22a7c333f0f5270a62584e2b47fc765150
[ "$QT_VERSION" ] || exit 1

[ "$QT_PATH" ] || QT_PATH=C:\\Qt

root_dir=$PWD
[ "$root_dir" != '/' ] || root_dir=""

# Init the package system
sudo apt update

echo
echo '--> Save the original installed packages list'
echo

dpkg --get-selections | cut -f 1 > /tmp/packages_orig.lst

echo
echo '--> Install the required packages to install Qt'
echo

sudo -E apt install -y --no-install-suggests --no-install-recommends curl ca-certificates wine64 wine
echo "$AQT_SHA256 -" > sum.txt && curl -fLs "$AQT_URL" | tee /tmp/aqt.exe | sha256sum -c sum.txt

echo
echo '--> Download & install the Qt library using aqt'
echo

wine /tmp/aqt.exe install-qt -O "$QT_PATH" windows desktop "$QT_VERSION" win64_mingw -m qtserialport
wine /tmp/aqt.exe install-tool -O "$QT_PATH" windows desktop tools_mingw90
wine /tmp/aqt.exe install-tool -O "$QT_PATH" windows desktop tools_cmake
wine /tmp/aqt.exe install-tool -O "$QT_PATH" windows desktop tools_ninja
wine /tmp/aqt.exe install-tool -O "$QT_PATH" windows desktop tools_opensslv3_x64
wine /tmp/aqt.exe install-tool -O "$QT_PATH" windows desktop tools_vcredist

echo
echo '--> Set Qt and Tools PATH for wine'
echo

cat - <<EOF > /tmp/path.reg
Windows Registry Editor Version 5.00

[HKEY_CURRENT_USER\\Environment]
"Path"="$(find "/home/user/.wine/drive_c/Qt" -type d -name bin -o -name Ninja | sed "s|/home/user/.wine/drive_c|C:|" | sed 's|/|\\\\|g' | tr '\n' ';')"
EOF

wine regedit /tmp/path.reg

echo
echo '--> Create helper functions'
echo

cat - <<\EOF | sudo -E tee /usr/local/bin/qt-cmake
#!/bin/sh
wine cmd /c qt-cmake "$@"
EOF

cat - <<\EOF | sudo -E tee /usr/local/bin/cmake
#!/bin/sh
wine cmake "$@"
EOF

cat - <<\EOF | sudo -E tee /usr/local/bin/ninja
#!/bin/sh
wine ninja "$@"
EOF

cat - <<\EOF | sudo -E tee /usr/local/bin/windeployqt
#!/bin/sh
wine windeployqt "$@"
EOF

sudo -E chmod +x /usr/local/bin/*

echo
echo '--> Restore the packages list to the original state'
echo

dpkg --get-selections | cut -f 1 > /tmp/packages_curr.lst
grep -Fxv -f /tmp/packages_orig.lst /tmp/packages_curr.lst | xargs sudo -E apt remove -y --purge

# Complete the cleaning

sudo -E apt -qq clean
sudo -E rm -rf /var/lib/apt/lists/* /tmp/wine-*
