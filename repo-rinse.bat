git clean -xfd
git submodule foreach --recursive git clean -xfd
rm -rf .git/modules
git init
git reset --hard
git submodule foreach --recursive git reset --hard
git submodule update --init --recursive
