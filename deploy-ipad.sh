#!/bin/bash
# Legion Stage iOS — Build & Deploy to iPad wirelessly
# Run this on the Mac via SSH from anywhere

set -e
export PATH="/opt/homebrew/bin:$PATH"

REPO_DIR="/Users/admin/legion-stage-ios"
IPAD_ID="4B80AD93-1089-53FE-AA74-1E4C6ABB05C5"
BUILD_DIR="$REPO_DIR/build-ios"

cd "$REPO_DIR"

echo ">> Pulling latest from repo..."
git pull origin main

echo ">> Configuring build..."
if [ ! -d "$BUILD_DIR" ]; then
    cmake -B "$BUILD_DIR" -G Xcode \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0
fi

echo ">> Building for iPad..."
xcodebuild -project "$BUILD_DIR/Sequencer.xcodeproj" \
    -scheme Sequencer \
    -configuration Release \
    -destination "id=$IPAD_ID" \
    -allowProvisioningUpdates \
    -allowProvisioningDeviceRegistration \
    CODE_SIGNING_ALLOWED=YES \
    -quiet

echo ">> Installing on iPad..."
xcrun devicectl device install app \
    --device "$IPAD_ID" \
    "$BUILD_DIR/Sequencer_artefacts/Release/Legion Stage.app"

echo ""
echo ">> Done! Legion Stage updated on iPad."
echo ">> Relaunch the app on your iPad to see changes."
