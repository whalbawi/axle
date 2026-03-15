function install-clang() {
    brew update
    brew install llvm@19
    sudo ln -s "$(brew --prefix llvm@19)/bin/clang++" "/usr/local/bin/clang++"
}

function install-clang-format() {
    brew update
    brew install llvm@19
    sudo ln -s "$(brew --prefix llvm@19)/bin/clang-format" "/usr/local/bin/clang-format"
    sudo ln -s "$(brew --prefix llvm@19)/bin/git-clang-format" "/usr/local/bin/git-clang-format"
}

function install-clang-tidy() {
    brew update
    brew install llvm@19
    sudo ln -s "$(brew --prefix llvm@19)/bin/clang-tidy" "/usr/local/bin/clang-tidy"
    sudo ln -s "$(brew --prefix llvm@19)/bin/run-clang-tidy" "/usr/local/bin/run-clang-tidy"
}

