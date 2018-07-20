#!/usr/bin/env bash

set -e

ROOT_DIR=dist
TARGET_DIR=${ROOT_DIR}/mac
BUILD_DIR=${TARGET_DIR}/intermediate
APP_NAME=Q2Pro
APP_DIR=${BUILD_DIR}/${APP_NAME}.app
CONTENT_DIR=${APP_DIR}/Contents
BIN_DIR=${CONTENT_DIR}/MacOS
RESOURCE_DIR=${CONTENT_DIR}/Resources
BASEQ2_BIN_DIR=${BIN_DIR}/baseq2
BASEQ2_RESOURCE_DIR=${RESOURCE_DIR}/baseq2
LIB_DIR=${CONTENT_DIR}/Library
VOLUME_NAME=${APP_NAME}

rm -rf ${BUILD_DIR}

mkdir -p ${ROOT_DIR}
mkdir -p ${BUILD_DIR}
mkdir -p ${APP_DIR}
mkdir -p ${CONTENT_DIR}
mkdir -p ${BIN_DIR}
mkdir -p ${RESOURCE_DIR}
mkdir -p ${LIB_DIR}
mkdir -p ${BASEQ2_BIN_DIR}
mkdir -p ${BASEQ2_RESOURCE_DIR}

cp scripts/apple/Info.plist ${CONTENT_DIR}
cp q2pro ${BIN_DIR}
cp q2proded ${BIN_DIR}
cp gamex86_64.so ${BASEQ2_BIN_DIR}

cp scripts/apple/q2pro.icns ${RESOURCE_DIR}
cp src/client/ui/q2pro.menu ${BASEQ2_RESOURCE_DIR}
tar zxf scripts/q2demo-baseq2.tar.gz -C ${BASEQ2_RESOURCE_DIR}

dylibbundler -b \
    -x "${BIN_DIR}/q2pro" \
    -x "${BIN_DIR}/q2proded" \
    -d "${LIB_DIR}" -of -p @executable_path/Library

ln -f -s /Applications ${BUILD_DIR}/Applications
hdiutil create ${TARGET_DIR}/q2pro-mac.dmg -srcfolder ${BUILD_DIR} -volname ${VOLUME_NAME}
