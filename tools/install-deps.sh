#!/usr/bin/env bash

SRC_DIR=$(dirname ${BASH_SOURCE[0]})
OS=$(uname)
if [ $OS = "Darwin" ]; then
    echo "Running on macOS"
    . $SRC_DIR/setup-darwin.sh
elif [ $OS = "Linux" ]; then
    echo "Running on Linux"
    if [ -f /etc/os-release ]; then
        grep -q '^ID=ubuntu$' /etc/os-release || exit 1
        cat /etc/os-release
    fi

    . $SRC_DIR/setup-ubuntu.sh
else
    echo "Unsupported OS: $OS"
    exit 1
fi

install-clang
install-clang-format
install-clang-tidy
