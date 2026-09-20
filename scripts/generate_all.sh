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

SCHEMA_PATH="schemas/transit/siri_core.xsd"

echo "================================================================================"
echo "🛠️  PolyXML Code Generation Showcase (All 7 Target Languages)"
echo "   Schema: ${SCHEMA_PATH}"
echo "================================================================================"

echo -e "\n[1/7] 🦀 Generating Rust (Zero-Copy Borrowed Slices & Inherent Codecs)..."
"${POLYXML_BIN}" generate "${SCHEMA_PATH}" \
  --lang rust \
  --zero-copy \
  --codecs \
  --out generated/rust

echo -e "\n[2/7] 🐍 Generating Python (Dataclass Backend with Inherent Codecs)..."
"${POLYXML_BIN}" generate "${SCHEMA_PATH}" \
  --lang python \
  --backend dataclass \
  --codecs \
  --out generated/python

echo -e "\n[3/7] 🐹 Generating Go (Dual XML & JSON Struct Tags)..."
"${POLYXML_BIN}" generate "${SCHEMA_PATH}" \
  --lang go \
  --package siri \
  --out generated/go

echo -e "\n[4/7] ⚡ Generating Modern C++20 (Header-Only Value Types & Concepts)..."
"${POLYXML_BIN}" generate "${SCHEMA_PATH}" \
  --lang cpp \
  --package "polyxml::generated" \
  --out generated/cpp

echo -e "\n[5/7] ☕ Generating Java 21+ (Records & Sealed Interfaces)..."
"${POLYXML_BIN}" generate "${SCHEMA_PATH}" \
  --lang java \
  --package "com.transit.siri" \
  --out generated/java

echo -e "\n[6/7] 🌐 Generating TypeScript 5+ (Typed Interfaces & Zod Validation)..."
"${POLYXML_BIN}" generate "${SCHEMA_PATH}" \
  --lang ts \
  --zod \
  --out generated/typescript

echo -e "\n[7/7] 🔷 Generating C# 12 / .NET 8 (Primary Constructor Records & Dual Attributes)..."
"${POLYXML_BIN}" generate "${SCHEMA_PATH}" \
  --lang csharp \
  --package "Transit.Siri" \
  --out generated/csharp

echo ""
echo "================================================================================"
echo "✅ Code generation finished across all 7 targets!"
echo "================================================================================"
