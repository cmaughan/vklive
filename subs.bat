git pull
git submodule update --init --recursive
cd external/zing
git checkout main
git pull
cmd /c subs.bat
cd ..
cd ..
cd zep
git checkout master
git pull
cd ..

