@echo off
call "%VS160COMNTOOLS%VsMSBuildCmd.bat"
devenv "master_VS2019.sln" /clean "Debug|x64" /Project "gsmaster"
devenv "master_VS2019.sln" /Build "Debug|x64" /Project "gsmaster"
copy /y readme.txt Debug\x64
copy /y Win32\x64\libcurl.dll Debug\x64
