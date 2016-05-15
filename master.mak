# Microsoft Developer Studio Generated NMAKE File, Based on master.dsp
!IF "$(CFG)" == ""
CFG=master - Win32 Debug
!MESSAGE No configuration specified. Defaulting to master - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "master - Win32 Release" && "$(CFG)" != "master - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "master.mak" CFG="master - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "master - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "master - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "master - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\master.exe" "$(OUTDIR)\master.bsc"


CLEAN :
	-@erase "$(INTDIR)\master.obj"
	-@erase "$(INTDIR)\master.sbr"
	-@erase "$(INTDIR)\performance.obj"
	-@erase "$(INTDIR)\performance.sbr"
	-@erase "$(INTDIR)\service.obj"
	-@erase "$(INTDIR)\service.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\master.bsc"
	-@erase "$(OUTDIR)\master.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\master.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\master.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\master.sbr" \
	"$(INTDIR)\performance.sbr" \
	"$(INTDIR)\service.sbr"

"$(OUTDIR)\master.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib advapi32.lib shell32.lib wsock32.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\master.pdb" /machine:I386 /out:"$(OUTDIR)\master.exe" 
LINK32_OBJS= \
	"$(INTDIR)\master.obj" \
	"$(INTDIR)\performance.obj" \
	"$(INTDIR)\service.obj"

"$(OUTDIR)\master.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "master - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\master.exe" "$(OUTDIR)\master.bsc"


CLEAN :
	-@erase "$(INTDIR)\master.obj"
	-@erase "$(INTDIR)\master.sbr"
	-@erase "$(INTDIR)\performance.obj"
	-@erase "$(INTDIR)\performance.sbr"
	-@erase "$(INTDIR)\service.obj"
	-@erase "$(INTDIR)\service.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\master.bsc"
	-@erase "$(OUTDIR)\master.exe"
	-@erase "$(OUTDIR)\master.ilk"
	-@erase "$(OUTDIR)\master.map"
	-@erase "$(OUTDIR)\master.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FAcs /Fa"$(INTDIR)\\" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\master.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\master.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\master.sbr" \
	"$(INTDIR)\performance.sbr" \
	"$(INTDIR)\service.sbr"

"$(OUTDIR)\master.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib advapi32.lib shell32.lib wsock32.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\master.pdb" /map:"$(INTDIR)\master.map" /debug /machine:I386 /out:"$(OUTDIR)\master.exe" 
LINK32_OBJS= \
	"$(INTDIR)\master.obj" \
	"$(INTDIR)\performance.obj" \
	"$(INTDIR)\service.obj"

"$(OUTDIR)\master.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("master.dep")
!INCLUDE "master.dep"
!ELSE 
!MESSAGE Warning: cannot find "master.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "master - Win32 Release" || "$(CFG)" == "master - Win32 Debug"
SOURCE=.\master.c

"$(INTDIR)\master.obj"	"$(INTDIR)\master.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\performance.c

"$(INTDIR)\performance.obj"	"$(INTDIR)\performance.sbr" : $(SOURCE) "$(INTDIR)"


SOURCE=.\service.c

"$(INTDIR)\service.obj"	"$(INTDIR)\service.sbr" : $(SOURCE) "$(INTDIR)"



!ENDIF 

