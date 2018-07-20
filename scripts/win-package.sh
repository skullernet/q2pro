#!/usr/bin/env bash

set -e

ROOT_DIR=dist
TARGET_DIR=${ROOT_DIR}/win
BUILD_DIR=${TARGET_DIR}/intermediate
BASEQ2_DIR=${BUILD_DIR}/baseq2
APP_NAME=q2pro

rm -rf ${BUILD_DIR}

mkdir -p ${ROOT_DIR}
mkdir -p ${BUILD_DIR}
mkdir -p ${BASEQ2_DIR}

cp q2pro.exe ${BUILD_DIR}
cp q2proded.exe ${BUILD_DIR}
cp gamex86_64.dll ${BASEQ2_DIR}

cp src/client/ui/q2pro.menu ${BASEQ2_DIR}
tar zxf scripts/q2demo-baseq2.tar.gz -C ${BASEQ2_DIR}

pushd ${BUILD_DIR}
zip -r ../${APP_NAME}-win.zip .
popd
