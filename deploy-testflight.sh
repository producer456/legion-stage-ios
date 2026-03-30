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

echo ">> Configuring build..."
if [ ! -f "$BUILD_DIR/Sequencer.xcodeproj/project.pbxproj" ]; then
    rm -rf "$BUILD_DIR"
    cmake -B "$BUILD_DIR" -G Xcode \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0
else
    echo "  Reusing existing CMake config"
fi

BUILD_NUMBER=$(date +%Y%m%d%H%M)
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

echo ">> Upload complete. Waiting for Apple to process..."

# Poll until build with our build number appears and is VALID
for i in $(seq 1 30); do
    sleep 10
    TOKEN=$(get_token)
    RESULT=$(curl -s -H "Authorization: Bearer $TOKEN" \
        "https://api.appstoreconnect.apple.com/v1/builds?sort=-uploadedDate&limit=1" \
        | python3 -c "
import sys,json
b=json.load(sys.stdin)['data'][0]
print(b['id'], b['attributes']['version'], b['attributes']['processingState'], b['attributes']['usesNonExemptEncryption'])
")
    BUILD_ID=$(echo "$RESULT" | awk '{print $1}')
    BUILD_VER=$(echo "$RESULT" | awk '{print $2}')
    STATE=$(echo "$RESULT" | awk '{print $3}')
    ENC=$(echo "$RESULT" | awk '{print $4}')
    echo "  Latest: v$BUILD_VER | state=$STATE | enc=$ENC | $BUILD_ID"
    if [ "$STATE" = "VALID" ] && [ "$BUILD_VER" = "$BUILD_NUMBER" ]; then
        break
    fi
    if [ "$i" = "30" ]; then
        echo "  Timeout waiting. Using latest available build."
    fi
done

echo ">> Setting encryption and adding to test group..."
TOKEN=$(get_token)

# Set no encryption
curl -s -X PATCH -H "Authorization: Bearer $TOKEN" -H "Content-Type: application/json" \
    -d "{\"data\":{\"type\":\"builds\",\"id\":\"$BUILD_ID\",\"attributes\":{\"usesNonExemptEncryption\":false}}}" \
    "https://api.appstoreconnect.apple.com/v1/builds/$BUILD_ID" > /dev/null

# Add to internal testers group
curl -s -X POST -H "Authorization: Bearer $TOKEN" -H "Content-Type: application/json" \
    -d "{\"data\":[{\"type\":\"builds\",\"id\":\"$BUILD_ID\"}]}" \
    "https://api.appstoreconnect.apple.com/v1/betaGroups/$INTERNAL_GROUP_ID/relationships/builds" > /dev/null

echo ""
echo ">> Done! Build v$BUILD_VER ($BUILD_ID) ready in TestFlight."
echo ">> Open TestFlight app on iPad to install."
