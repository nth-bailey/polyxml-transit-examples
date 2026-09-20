#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${ROOT_DIR}"

POLYXML_BIN="polyxml"
if [ -f "${ROOT_DIR}/../PolyXML/target/release/polyxml" ] && [ -f "${ROOT_DIR}/../PolyXML/target/debug/polyxml" ]; then
    if [ "${ROOT_DIR}/../PolyXML/target/release/polyxml" -nt "${ROOT_DIR}/../PolyXML/target/debug/polyxml" ]; then
        POLYXML_BIN="${ROOT_DIR}/../PolyXML/target/release/polyxml"
    else
        POLYXML_BIN="${ROOT_DIR}/../PolyXML/target/debug/polyxml"
    fi
elif [ -f "${ROOT_DIR}/../PolyXML/target/release/polyxml" ]; then
    POLYXML_BIN="${ROOT_DIR}/../PolyXML/target/release/polyxml"
elif [ -f "${ROOT_DIR}/../PolyXML/target/debug/polyxml" ]; then
    POLYXML_BIN="${ROOT_DIR}/../PolyXML/target/debug/polyxml"
elif command -v polyxml &>/dev/null; then
    POLYXML_BIN="polyxml"
fi

echo "================================================================================"
echo "🚍 PolyXML CLI Streaming & Schema-Directed Transcoder Demo (Transit)"
echo "================================================================================"

TMP_DIR="$(mktemp -d /tmp/polyxml-transit-transcode-XXXXXX)"
trap 'rm -rf "${TMP_DIR}"' EXIT

echo -e "\n[1] Input: Google GTFS-Realtime Live Vehicle Telemetry (JSON)"
head -n 25 data/gtfs_realtime_vehicle.json
echo "..."

echo -e "\n[2] Streaming Pipe: CEN SIRI XML -> Canonical JSON via PolyXML CLI:"
cat data/siri_vehicle_monitoring.xml | "${POLYXML_BIN}" transcode --to json --pretty > "${TMP_DIR}/siri_pipe.json"
head -n 30 "${TMP_DIR}/siri_pipe.json"
echo "..."

echo -e "\n[3] Streaming Pipe: Canonical JSON -> CEN SIRI XML via PolyXML CLI:"
cat "${TMP_DIR}/siri_pipe.json" | "${POLYXML_BIN}" transcode --to xml --root Siri --pretty > "${TMP_DIR}/siri_pipe.xml"
head -n 30 "${TMP_DIR}/siri_pipe.xml"
echo "..."

echo -e "\n[4] Schema-Guided Transcoding: CEN SIRI XML -> Strongly-Typed JSON (XSD-Driven):"
"${POLYXML_BIN}" transcode --schema schemas/transit/siri_core.xsd --pretty data/siri_vehicle_monitoring.xml -o "${TMP_DIR}/siri_typed.json"
head -n 30 "${TMP_DIR}/siri_typed.json"
echo "..."

echo -e "\n[5] Validating CEN SIRI v2.0 XML Schema:"
"${POLYXML_BIN}" validate schemas/transit/siri_core.xsd

echo -e "\n✅ PolyXML CLI Bidirectional Streaming & Schema-Directed Transcoding Complete!"
