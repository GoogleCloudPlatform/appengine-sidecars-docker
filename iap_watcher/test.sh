#!/bin/bash
set -euo pipefail
IMG=iap-watcher-tester
echo "Building IAP Watcher"
docker build -t "$IMG" --target iap-watcher-tester .
echo "Running IAP Watcher tests..."
docker run --rm "$IMG"