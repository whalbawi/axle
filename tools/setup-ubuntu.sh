function install-clang() {
    sudo apt update
    sudo apt install -y clang-19
    sudo ln -s $(which clang++-19) "/usr/local/bin/clang++"
    sudo ln -s $(which clang-19) "/usr/local/bin/clang"
}

function install-clang-format() {
    sudo apt update
    sudo apt install -y clang-format-19
    sudo ln -s $(which clang-format-19) "/usr/local/bin/clang-format"
    sudo ln -s $(which git-clang-format-19) "/usr/local/bin/git-clang-format"
}

function install-clang-tidy() {
    sudo apt update
    sudo apt install -y clang-tidy-19
    sudo ln -s $(which clang-tidy-19) "/usr/local/bin/clang-tidy"
    sudo ln -s $(which run-clang-tidy-19) "/usr/local/bin/run-clang-tidy"
}

