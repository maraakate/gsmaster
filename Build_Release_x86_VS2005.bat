@echo off
call vcvars32.bat
call "%VS80COMNTOOLS%vsvars32.bat"
devenv "master_VS2005.sln" /clean "Release|Win32" /Project "master"
devenv "master_VS2005.sln" /Build "Release|Win32" /Project "master"
copy /y readme.txt Release\x86
