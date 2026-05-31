!include "MUI2.nsh"

Name "foobar2000-obs"
OutFile "foobar2000-obs-installer.exe"
InstallDir "$PROGRAMFILES64\obs-studio"
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

    ; Try InstallLocation from registry
    ReadRegStr $OBSPath HKLM \
    "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio" \
    "InstallLocation"
    IfFileExists "$OBSPath\bin\64bit\obs64.exe" setpath

    ; Try DisplayIcon (full path to obs64.exe)
    ReadRegStr $0 HKLM \
    "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio" \
    "DisplayIcon"
    StrCpy $1 $0 20 -20
    StrCmp $1 "\bin\64bit\obs64.exe" 0 try_wow
    StrCpy $OBSPath $0 -20
    IfFileExists "$OBSPath\bin\64bit\obs64.exe" setpath

try_wow:
    ; Try WOW6432Node InstallLocation
    ReadRegStr $OBSPath HKLM \
    "SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio" \
    "InstallLocation"
    IfFileExists "$OBSPath\bin\64bit\obs64.exe" setpath

    ; Try WOW6432Node DisplayIcon
    ReadRegStr $0 HKLM \
    "SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio" \
    "DisplayIcon"
    StrCpy $1 $0 20 -20
    StrCmp $1 "\bin\64bit\obs64.exe" 0 try_pf64
    StrCpy $OBSPath $0 -20
    IfFileExists "$OBSPath\bin\64bit\obs64.exe" setpath

try_pf64:
    StrCpy $OBSPath "$PROGRAMFILES64\obs-studio"
    IfFileExists "$OBSPath\bin\64bit\obs64.exe" setpath

try_pf32:
    StrCpy $OBSPath "$PROGRAMFILES\obs-studio"
    IfFileExists "$OBSPath\bin\64bit\obs64.exe" setpath

    Goto done

setpath:
    StrCpy $INSTDIR "$OBSPath"

done:

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