#!/bin/bash

if [ "$1" == "" ] ; then
CONFIG=Debug
else
CONFIG=$1
fi

source config.sh $CONFIG

if [[ "$OSTYPE" == "darwin"* ]]; then
PACKAGE_TYPE=osx
else
PACKAGE_TYPE=linux
fi

cd build
cmake --build . --config $CONFIG
cd ..

echo "Built $CONFIG for $PACKAGE_TYPE"
