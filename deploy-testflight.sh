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

cd "$REPO_DIR"

echo ">> Pulling latest from repo..."
git pull origin main

echo ">> Configuring build..."
rm -rf "$BUILD_DIR"
cmake -B "$BUILD_DIR" -G Xcode \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0

# Auto-increment build number based on timestamp
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

echo ">> Waiting for build to process..."

# Poll until a new build appears and is VALID (up to 5 minutes)
echo ">> Waiting for new build to appear..."
PREV_BUILD_ID=$(curl -s -H "Authorization: Bearer $(python3 -c "
import jwt, time
key = open('$HOME/.appstoreconnect/private_keys/AuthKey_$API_KEY.p8').read()
payload = {'iss': '$API_ISSUER', 'iat': int(time.time()), 'exp': int(time.time()) + 1200, 'aud': 'appstoreconnect-v1'}
print(jwt.encode(payload, key, algorithm='ES256', headers={'kid': '$API_KEY'}))
")" "https://api.appstoreconnect.apple.com/v1/builds?sort=-uploadedDate&limit=1" \
    | python3 -c "import sys,json; print(json.load(sys.stdin)['data'][0]['id'])")
echo "  Previous latest: $PREV_BUILD_ID"

for i in $(seq 1 30); do
    sleep 10
    TOKEN=$(python3 -c "
import jwt, time
key = open('$HOME/.appstoreconnect/private_keys/AuthKey_$API_KEY.p8').read()
payload = {'iss': '$API_ISSUER', 'iat': int(time.time()), 'exp': int(time.time()) + 1200, 'aud': 'appstoreconnect-v1'}
print(jwt.encode(payload, key, algorithm='ES256', headers={'kid': '$API_KEY'}))
")
    RESULT=$(curl -s -H "Authorization: Bearer $TOKEN" \
        "https://api.appstoreconnect.apple.com/v1/builds?sort=-uploadedDate&limit=1" \
        | python3 -c "import sys,json; d=json.load(sys.stdin)['data'][0]; print(d['id'],d['attributes']['processingState'])")
    CURRENT_ID=$(echo "$RESULT" | cut -d' ' -f1)
    STATE=$(echo "$RESULT" | cut -d' ' -f2)
    echo "  Build: $CURRENT_ID state: $STATE"
    if [ "$CURRENT_ID" != "$PREV_BUILD_ID" ] && [ "$STATE" = "VALID" ]; then
        break
    fi
done

echo ">> Setting encryption compliance and adding to test group..."
TOKEN=$(python3 -c "
import jwt, time
key = open('$HOME/.appstoreconnect/private_keys/AuthKey_$API_KEY.p8').read()
payload = {'iss': '$API_ISSUER', 'iat': int(time.time()), 'exp': int(time.time()) + 1200, 'aud': 'appstoreconnect-v1'}
print(jwt.encode(payload, key, algorithm='ES256', headers={'kid': '$API_KEY'}))
")

BUILD_ID=$(curl -s -H "Authorization: Bearer $TOKEN" \
    "https://api.appstoreconnect.apple.com/v1/builds?sort=-uploadedDate&limit=1" \
    | python3 -c "import sys,json; print(json.load(sys.stdin)['data'][0]['id'])")

# Set no encryption
curl -s -X PATCH -H "Authorization: Bearer $TOKEN" -H "Content-Type: application/json" \
    -d "{\"data\":{\"type\":\"builds\",\"id\":\"$BUILD_ID\",\"attributes\":{\"usesNonExemptEncryption\":false}}}" \
    "https://api.appstoreconnect.apple.com/v1/builds/$BUILD_ID" > /dev/null

# Add to internal testers group
curl -s -X POST -H "Authorization: Bearer $TOKEN" -H "Content-Type: application/json" \
    -d "{\"data\":[{\"type\":\"builds\",\"id\":\"$BUILD_ID\"}]}" \
    "https://api.appstoreconnect.apple.com/v1/betaGroups/$INTERNAL_GROUP_ID/relationships/builds" > /dev/null

echo ""
echo ">> Done! Build $BUILD_ID uploaded and ready in TestFlight."
echo ">> Open TestFlight app on iPad to install."
