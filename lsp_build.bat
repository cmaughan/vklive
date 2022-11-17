set CURRENT_DIR=%CD%
set PATH=%PATH%;%VCINSTALLDIR%\Tools\Llvm\x64\bin\
more %PATH%
mkdir build_llvm > nul
cd build_llvm
cmake -G "Ninja" -DCMAKE_EXPORT_COMPILE_COMMANDS=1 ..\
cd "%CURRENT_DIR%"
copy build_llvm\compile_com* build

