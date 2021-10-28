@ECHO OFF
TASKKILL /F /T /IM MSDEV.EXE 
TASKKILL /F /T /IM MSDEV.COM
msdev master.dsw /MAKE "master - Win32 Debug" /CLEAN
msdev master.dsw /MAKE "master - Win32 Debug" /BUILD
copy /y readme.txt Debug\x86
