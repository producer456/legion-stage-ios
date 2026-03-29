#!/bin/bash
# Legion Stage iOS — Build & Upload to TestFlight
# Run this on the Mac to push a new build to TestFlight

set -e
export PATH="/opt/homebrew/bin:$PATH"

REPO_DIR="/Users/admin/legion-stage-ios"
BUILD_DIR="$REPO_DIR/build-ios"
TEAM_ID="9TUXM4MBAV"
ARCHIVE_PATH="/tmp/LegionStage.xcarchive"
EXPORT_PATH="/tmp/LegionStageExport"

cd "$REPO_DIR"

echo ">> Pulling latest from repo..."
git pull origin main

echo ">> Configuring build..."
rm -rf "$BUILD_DIR"
cmake -B "$BUILD_DIR" -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0

echo ">> Archiving..."
xcodebuild -project "$BUILD_DIR/Sequencer.xcodeproj" \
    -scheme Sequencer \
    -configuration Release \
    -destination "generic/platform=iOS" \
    -archivePath "$ARCHIVE_PATH" \
    archive \
    -allowProvisioningUpdates \
    CODE_SIGNING_ALLOWED=YES \
    DEVELOPMENT_TEAM="$TEAM_ID" \
    CODE_SIGN_STYLE=Automatic \
    -quiet

echo ">> Uploading to TestFlight..."
cat > /tmp/ExportOptions.plist << 'EOF'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>method</key>
    <string>app-store-connect</string>
    <key>teamID</key>
    <string>9TUXM4MBAV</string>
    <key>signingStyle</key>
    <string>automatic</string>
    <key>uploadBitcode</key>
    <false/>
    <key>uploadSymbols</key>
    <true/>
    <key>destination</key>
    <string>upload</string>
</dict>
</plist>
EOF

rm -rf "$EXPORT_PATH"
xcodebuild -exportArchive \
    -archivePath "$ARCHIVE_PATH" \
    -exportOptionsPlist /tmp/ExportOptions.plist \
    -exportPath "$EXPORT_PATH" \
    -allowProvisioningUpdates

echo ""
echo ">> Done! Build uploaded to TestFlight."
echo ">> It will be available on your iPad in ~15 minutes."
echo ">> Open TestFlight app on iPad to install."
