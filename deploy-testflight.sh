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
API_KEY="FV5WR6A335"
API_ISSUER="063d077f-1dbb-4904-8ead-515fe477da68"
INTERNAL_GROUP_ID="f7bfbd76-3c8c-4411-9c27-af47b73d7c2e"
KEY_FILE="$HOME/.appstoreconnect/private_keys/AuthKey_${API_KEY}.p8"

get_token() {
    python3 -c "
import jwt, time
key = open('${KEY_FILE}').read()
payload = {'iss': '${API_ISSUER}', 'iat': int(time.time()), 'exp': int(time.time()) + 1200, 'aud': 'appstoreconnect-v1'}
print(jwt.encode(payload, key, algorithm='ES256', headers={'kid': '${API_KEY}'}))
"
}

cd "$REPO_DIR"

echo ">> Pulling latest from repo..."
git pull origin main

# Epoch seconds — fits in uint32 (Apple's CFBundleVersion segment
# limit) AND monotonically increases.  An older format (date
# +%Y%m%d%H%M) overflowed and silently fell back to a stale value,
# so TestFlight refused to surface the new build as an update.
BUILD_NUMBER=$(date +%s)

echo ">> Configuring build (BUILD_NUMBER=$BUILD_NUMBER)..."
# Always wipe the cached config so the new build number is baked
# into the JUCE-generated Info.plist.  Reusing the cache caused
# silent CFBundleVersion regressions that confused TestFlight.
rm -rf "$BUILD_DIR"
cmake -B "$BUILD_DIR" -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
    -DLEGION_BUILD_VERSION="$BUILD_NUMBER"

echo ">> Archiving (build $BUILD_NUMBER)..."
xcodebuild -project "$BUILD_DIR/Sequencer.xcodeproj" \
    CURRENT_PROJECT_VERSION="$BUILD_NUMBER" \
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
cat > /tmp/ExportOptions.plist << 'PLISTEOF'
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
PLISTEOF

rm -rf "$EXPORT_PATH"
xcodebuild -exportArchive \
    -archivePath "$ARCHIVE_PATH" \
    -exportOptionsPlist /tmp/ExportOptions.plist \
    -exportPath "$EXPORT_PATH" \
    -allowProvisioningUpdates

echo ">> Upload complete. Waiting for Apple to process and unblocking TestFlight..."
python3 ~/.appstoreconnect/tf-publish.py com.dev.legionstage
