#!/bin/bash
set -e

# Support both podman and docker
DOCKER_CMD="docker"
if command -v podman &> /dev/null; then
    DOCKER_CMD="podman"
fi

IMAGE_NAME="qt6-mingw"

echo "Building using ${DOCKER_CMD} with image ${IMAGE_NAME}..."

# We need pybind11 locally because CMake under Wine crashes when running git clone
if [ ! -d "pybind11-src" ]; then
    echo "Cloning pybind11 to bypass Wine git issues..."
    git clone --depth 1 --branch v2.12.0 https://github.com/pybind/pybind11.git pybind11-src
fi

mkdir -p build_windows

# Run the build inside the container.
# We map the current directory to /workspace.
# Since the container runs as 'user', we adjust ownership inside
# to avoid permission denied errors, then restore ownership afterwards.

${DOCKER_CMD} run --rm \
    -v "$PWD:/workspace" \
    -w "/workspace/build_windows" \
    "${IMAGE_NAME}" \
    bash -c "\
      sudo chown -R user:user /workspace/build_windows /workspace/pybind11-src && \
      qt-cmake -GNinja -DCMAKE_BUILD_TYPE=Release \
        -DPYBIND11_FINDPYTHON=ON \
        -DFETCHCONTENT_SOURCE_DIR_PYBIND11=/workspace/pybind11-src .. && \
      ninja && \
      windeployqt --compiler-runtime serial-terminal-qt.exe && \
      # Ensure libssp-0.dll is present (often missed by windeployqt)
      find /home/user/.wine/drive_c/Qt/Tools -name \"libssp-0.dll\" -exec cp {} . \; && \
      # Copy python runtime DLL and libraries
      find /home/user/.wine/drive_c/Qt/Tools -name \"libpython3.9.dll\" -exec cp {} . \; && \
      PYTHON_LIB_DIR=\$(find /home/user/.wine/drive_c/Qt/Tools -type d -path \"*/opt/lib/python3.9\" | head -n 1) && \
      if [ -n \"\$PYTHON_LIB_DIR\" ]; then mkdir -p ./lib/python3.9 && cp -r \"\$PYTHON_LIB_DIR\"/* ./lib/python3.9/; fi && \
      sudo chown -R \$(stat -c '%u:%g' /workspace) /workspace/build_windows /workspace/pybind11-src
    "

echo "Creating SFX package..."
cat << 'EOF' > package_sfx.sh
#!/bin/bash
set -e
sudo apt update -qq && sudo apt install -y -qq curl p7zip-full > /dev/null

# Download the LZMA SDK to get the silent installer SFX stub (7zSD.sfx)
curl -sSL -o /tmp/lzma.7z https://www.7-zip.org/a/lzma2301.7z
7z x /tmp/lzma.7z -o/tmp/lzma bin/7zSD.sfx > /dev/null

# Download rcedit to patch the executable manifest
curl -sSL -o /tmp/rcedit.exe https://github.com/electron/rcedit/releases/download/v2.0.0/rcedit-x64.exe

# Create an execution manifest to bypass UAC heuristic for installers
cat << 'XML' > /tmp/manifest.xml
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">
    <security>
      <requestedPrivileges>
        <requestedExecutionLevel level="asInvoker" uiAccess="false"/>
      </requestedPrivileges>
    </security>
  </trustInfo>
</assembly>
XML

# Embed the manifest using wine
export WINEDEBUG=-all
wine /tmp/rcedit.exe /tmp/lzma/bin/7zSD.sfx --application-manifest /tmp/manifest.xml

cd /workspace/build_windows

# Define the exact files/folders needed for deployment
DEPLOY_FILES="serial-terminal-qt.exe *.dll lib/ generic/ iconengines/ imageformats/ networkinformation/ platforms/ styles/ tls/ translations/"

# Create the standard dynamic ZIP archive
sudo 7z a -tzip serial-monitor-windows.zip $DEPLOY_FILES > /dev/null

sudo bash -c "echo -e \";!@Install@!UTF-8!\nTitle=\\\"Serial Monitor\\\"\nRunProgram=\\\"serial-terminal-qt.exe\\\"\n;!@InstallEnd@!\" > config.txt"
sudo 7z a -r release.7z $DEPLOY_FILES > /dev/null
sudo bash -c "cat /tmp/lzma/bin/7zSD.sfx config.txt release.7z > serial-monitor-portable.exe"
sudo rm release.7z config.txt
EOF

${DOCKER_CMD} run --rm -v "$PWD:/workspace" "${IMAGE_NAME}" bash /workspace/package_sfx.sh
rm package_sfx.sh

echo "Build complete. Artifacts are in build_windows/ :"
echo " - serial-monitor-portable.exe (Single Portable Executable)"
echo " - serial-monitor-windows.zip (Standard Extracted ZIP)"
