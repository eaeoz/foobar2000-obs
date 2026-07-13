!include "MUI2.nsh"

Name "foobar2000-obs by Sedat ERGOZ"
OutFile "foobar2000-obs-installer.exe"
InstallDir "$PROGRAMFILES64\obs-studio"
RequestExecutionLevel admin
BrandingText "foobar2000-obs by Sedat ERGOZ"

VIProductVersion "1.0.0.0"
VIAddVersionKey "ProductName" "foobar2000-obs"
VIAddVersionKey "CompanyName" "Sedat ERGOZ"
VIAddVersionKey "FileDescription" "foobar2000-obs - OBS plugin by Sedat ERGOZ"
VIAddVersionKey "ProductVersion" "1.0.0"
VIAddVersionKey "Comments" "foobar2000-obs by Sedat ERGOZ - sedatergoz@gmail.com"

Var OBSPath
Var FB2KPath

!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit

    SetRegView 64

    ; --- Detect OBS Studio ---
    ; Try InstallLocation from registry
    ReadRegStr $OBSPath HKLM \
    "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio" \
    "InstallLocation"
    IfFileExists "$OBSPath\bin\64bit\obs64.exe" obs_found

    ; Try DisplayIcon (full path to obs64.exe)
    ReadRegStr $0 HKLM \
    "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio" \
    "DisplayIcon"
    StrCpy $1 $0 20 -20
    StrCmp $1 "\bin\64bit\obs64.exe" 0 obs_try_wow
    StrCpy $OBSPath $0 -20
    IfFileExists "$OBSPath\bin\64bit\obs64.exe" obs_found

obs_try_wow:
    ; Try WOW6432Node InstallLocation
    ReadRegStr $OBSPath HKLM \
    "SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio" \
    "InstallLocation"
    IfFileExists "$OBSPath\bin\64bit\obs64.exe" obs_found

    ; Try WOW6432Node DisplayIcon
    ReadRegStr $0 HKLM \
    "SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio" \
    "DisplayIcon"
    StrCpy $1 $0 20 -20
    StrCmp $1 "\bin\64bit\obs64.exe" 0 obs_try_pf64
    StrCpy $OBSPath $0 -20
    IfFileExists "$OBSPath\bin\64bit\obs64.exe" obs_found

obs_try_pf64:
    StrCpy $OBSPath "$PROGRAMFILES64\obs-studio"
    IfFileExists "$OBSPath\bin\64bit\obs64.exe" obs_found

    StrCpy $OBSPath "$PROGRAMFILES\obs-studio"
    IfFileExists "$OBSPath\bin\64bit\obs64.exe" obs_found

    Goto obs_done

obs_found:
    StrCpy $INSTDIR "$OBSPath"

obs_done:

    ; --- Detect foobar2000 v2 ---
    ReadRegStr $FB2KPath HKCU \
    "Software\foobar2000\foil" ""
    IfFileExists "$FB2KPath\foobar2000.exe" fb2k_done

    ReadRegStr $FB2KPath HKLM \
    "Software\foobar2000\foil" ""
    IfFileExists "$FB2KPath\foobar2000.exe" fb2k_done

    ; Try common paths
    StrCpy $FB2KPath "$LOCALAPPDATA\foobar2000-v2"
    IfFileExists "$FB2KPath\foobar2000.exe" fb2k_done

    StrCpy $FB2KPath "$LOCALAPPDATA\foobar2000"
    IfFileExists "$FB2KPath\foobar2000.exe" fb2k_done

    StrCpy $FB2KPath "$PROGRAMFILES64\foobar2000"
    IfFileExists "$FB2KPath\foobar2000.exe" fb2k_done

    StrCpy $FB2KPath "$PROGRAMFILES\foobar2000"
    IfFileExists "$FB2KPath\foobar2000.exe" fb2k_done

fb2k_done:

FunctionEnd

Section "Install"

    StrCpy $0 "$INSTDIR"

    IfFileExists "$0\bin\64bit\obs64.exe" obs_ok obs_fail

obs_fail:
    MessageBox MB_ICONSTOP "Invalid OBS folder selected"
    Abort

obs_ok:

    ; --------------------------
    ; INSTALL OBS PLUGIN
    ; --------------------------
    SetOutPath "$0\obs-plugins\64bit"
    File "build_x64\rundir\RelWithDebInfo\foobar2000-obs.dll"
    File "build_x64\rundir\RelWithDebInfo\foobar2000-obs.pdb"

    SetOutPath "$0\data\obs-plugins\foobar2000-obs\locale"
    File "build_x64\rundir\RelWithDebInfo\foobar2000-obs\locale\en-US.ini"

    ; --------------------------
    ; INSTALL FOOBAR2000 BRIDGE COMPONENT
    ; --------------------------
    StrCpy $1 "$FB2KPath"

    IfFileExists "$1\foobar2000.exe" fb2k_ok fb2k_skip

fb2k_skip:
    MessageBox MB_OK "foobar2000 not found. Skipping bridge component.$\n$\nTo install manually, copy foo_obsbridge.dll to your foobar2000 components folder."
    Goto done

fb2k_ok:

    ; Components folder is always next to foobar2000.exe (works for portable + standard)
    StrCpy $2 "$FB2KPath\components"
    CreateDirectory "$2"
    SetOutPath "$2"
    File "build_x64\out\foo_obsbridge.dll"

    MessageBox MB_OK "foobar2000 bridge component installed.$\n$\nRestart foobar2000 to activate."

done:

SectionEnd
