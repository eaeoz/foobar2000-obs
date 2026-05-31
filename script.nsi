!include "MUI2.nsh"

Name "foobar2000-obs"
OutFile "foobar2000-obs-installer.exe"
RequestExecutionLevel admin

VIProductVersion "1.0.0.0"
VIAddVersionKey "ProductName" "foobar2000-obs"
VIAddVersionKey "CompanyName" "Sedat ERGOZ"
VIAddVersionKey "FileDescription" "OBS Plugin for foobar2000 integration"
VIAddVersionKey "ProductVersion" "1.0.0"
VIAddVersionKey "Comments" "sedatergoz@gmail.com"

Var OBSPath

!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit

    SetRegView 64

    ReadRegStr $OBSPath HKLM \
    "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio" \
    "InstallLocation"

    IfFileExists "$OBSPath\bin\64bit\obs64.exe" 0 try_pf64
    Goto done

try_pf64:
    StrCpy $OBSPath "$PROGRAMFILES64\obs-studio"
    IfFileExists "$OBSPath\bin\64bit\obs64.exe" done try_pf32

try_pf32:
    StrCpy $OBSPath "$PROGRAMFILES\obs-studio"
    IfFileExists "$OBSPath\bin\64bit\obs64.exe" done fail

fail:
    StrCpy $OBSPath ""

done:
    StrCpy $INSTDIR "$OBSPath"

FunctionEnd

Section "Install"

    StrCpy $0 "$INSTDIR"

    IfFileExists "$0\bin\64bit\obs64.exe" ok fail

fail:
    MessageBox MB_ICONSTOP "Invalid OBS folder selected"
    Abort

ok:

    ; --------------------------
    ; INSTALL DLL ONLY (safe)
    ; --------------------------
    SetOutPath "$0\obs-plugins\64bit"
    File /r "build_x64\rundir\RelWithDebInfo\*.dll"

SectionEnd