#!/bin/bash
# In Your Phase bauen, testen, installieren und als Audio Unit prüfen.
# Start: Doppelklick im Finder, oder im Terminal:  bash InYourPhase-bauen.command
#
# Braucht nur die Xcode Command Line Tools. CMake und JUCE lädt das Skript selbst.

set -euo pipefail
cd "$(dirname "$0")"

JUCE_VERSION="8.0.15"
CMAKE_VERSION="3.30.5"
TOOLS_DIR="$PWD/.werkzeuge"
BUILD_DIR="$PWD/build-mac"

pause_and_exit() {
    echo
    read -n 1 -s -r -p "Taste drücken zum Schließen …" || true
    echo
    exit "$1"
}
trap 'echo; echo "Abgebrochen. Die Meldung oben zeigt, woran es lag."; pause_and_exit 1' ERR

echo "=================================================="
echo " In Your Phase bauen"
echo "=================================================="

# 1. Compiler und git
if ! xcode-select -p >/dev/null 2>&1; then
    echo "Die Xcode Command Line Tools fehlen."
    echo "Gleich öffnet sich ein Installationsfenster. Bitte installieren"
    echo "und danach dieses Skript noch einmal starten."
    xcode-select --install || true
    pause_and_exit 1
fi
echo "✓ Xcode Command Line Tools"

# 2. CMake 3.22 oder neuer
CMAKE=""
if command -v cmake >/dev/null 2>&1; then
    version=$(cmake --version | head -1 | awk '{print $3}')
    major=${version%%.*}
    rest=${version#*.}
    minor=${rest%%.*}
    if [ "$major" -gt 3 ] || { [ "$major" -eq 3 ] && [ "$minor" -ge 22 ]; }; then
        CMAKE="cmake"
    fi
fi
if [ -z "$CMAKE" ]; then
    CMAKE="$TOOLS_DIR/cmake-$CMAKE_VERSION-macos-universal/CMake.app/Contents/bin/cmake"
    if [ ! -x "$CMAKE" ]; then
        echo "Lade CMake $CMAKE_VERSION herunter …"
        mkdir -p "$TOOLS_DIR"
        curl -fL --progress-bar -o "$TOOLS_DIR/cmake.tar.gz" \
            "https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION-macos-universal.tar.gz"
        tar -xzf "$TOOLS_DIR/cmake.tar.gz" -C "$TOOLS_DIR"
        rm -f "$TOOLS_DIR/cmake.tar.gz"
    fi
fi
echo "✓ CMake ($("$CMAKE" --version | head -1))"

# 3. JUCE
if [ ! -f JUCE/CMakeLists.txt ]; then
    echo "Lade JUCE $JUCE_VERSION herunter …"
    rm -rf JUCE
    git clone --depth 1 --branch "$JUCE_VERSION" https://github.com/juce-framework/JUCE.git JUCE
fi
echo "✓ JUCE $JUCE_VERSION"

# 4. Bauen. Audio Unit und VST3 landen automatisch in ~/Library/Audio/Plug-Ins
echo
echo "Baue In Your Phase … (beim ersten Mal dauert das ein paar Minuten)"
"$CMAKE" -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DJUCE_DIR="$PWD/JUCE"
"$CMAKE" --build "$BUILD_DIR" --config Release \
    --target InYourPhase_AU InYourPhase_VST3 InYourPhase_Standalone InYourPhaseTests -j "$(sysctl -n hw.ncpu)"

# 5. Tests
echo
echo "Führe Tests aus …"
TESTS=$(find "$BUILD_DIR" -path "*InYourPhaseTests_artefacts*" -type f -name InYourPhaseTests -perm -111 | head -1)
"$TESTS" | tail -1

# 6. Audio-Unit-Cache auffrischen und mit Apples auval prüfen
echo
echo "Prüfe die Audio Unit mit auval …"
killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
sleep 2
if auval -v aufx Iyp1 Mnsh > "$BUILD_DIR/auval.txt" 2>&1 && grep -q "AU VALIDATION SUCCEEDED" "$BUILD_DIR/auval.txt"; then
    echo "✓ AU-Validierung bestanden"
else
    echo "✗ AU-Validierung fehlgeschlagen, Details in $BUILD_DIR/auval.txt:"
    tail -25 "$BUILD_DIR/auval.txt"
    pause_and_exit 1
fi

# 7. Eigenständige App nach ~/Applications
APP=$(find "$BUILD_DIR" -path "*InYourPhase_artefacts*" -type d -name "In Your Phase.app" | head -1)
mkdir -p "$HOME/Applications"
rm -rf "$HOME/Applications/In Your Phase.app"
ditto "$APP" "$HOME/Applications/In Your Phase.app"

echo
echo "=================================================="
echo " Fertig"
echo "=================================================="
echo "Audio Unit:  ~/Library/Audio/Plug-Ins/Components/In Your Phase.component"
echo "VST3:        ~/Library/Audio/Plug-Ins/VST3/In Your Phase.vst3"
echo "App:         ~/Applications/In Your Phase.app"
echo
echo "Ableton: Einstellungen → Plug-Ins → neu scannen, dann unter"
echo "         Plug-Ins → moonshroom studio → In Your Phase."
echo "SignalFlow: „Plugins scannen“ drücken."
pause_and_exit 0
