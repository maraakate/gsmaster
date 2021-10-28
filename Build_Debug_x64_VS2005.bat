@echo off
call vcvars32.bat
call "%VS80COMNTOOLS%vsvars32.bat"
devenv "master_VS2005.sln" /clean "Debug|x64" /Project "master"
devenv "master_VS2005.sln" /Build "Debug|x64" /Project "master"
copy /y readme.txt Debug\x64
