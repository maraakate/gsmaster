@echo off
call "%VS160COMNTOOLS%VsMSBuildCmd.bat"
devenv "master_VS2019.sln" /clean "Release|x64" /Project "gsmaster"
devenv "master_VS2019.sln" /Build "Release|x64" /Project "gsmaster"
copy /y readme.txt Release\x64
copy /y Win32\x64\libcurl.dll Release\x64
