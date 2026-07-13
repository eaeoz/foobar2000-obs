!include "MUI2.nsh"
!include "nsDialogs.nsh"

Name "foobar2000-obs by Sedat ERGOZ"
OutFile "foobar2000-obs-installer.exe"
RequestExecutionLevel user
BrandingText "foobar2000-obs by Sedat ERGOZ"

VIProductVersion "2.0.0.0"
VIAddVersionKey "ProductName" "foobar2000-obs"
VIAddVersionKey "CompanyName" "Sedat ERGOZ"
VIAddVersionKey "FileDescription" "foobar2000-obs - OBS plugin by Sedat ERGOZ"
VIAddVersionKey "ProductVersion" "2.0.0"
VIAddVersionKey "Comments" "foobar2000-obs by Sedat ERGOZ - sedatergoz@gmail.com"

Var OBSPath
Var FB2KPath
Var FB2KDirHWND
Var FB2KDirText

!define MUI_PAGE_CUSTOMFUNCTION_LEAVE obs_page_leave
!insertmacro MUI_PAGE_DIRECTORY
Page custom fb2k_page_show fb2k_page_leave
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Function .onInit

    SetRegView 64

    ; --- Detect OBS Studio ---
    StrCpy $OBSPath ""

    ReadRegStr $OBSPath HKLM \
    "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio" \
    "InstallLocation"
    IfFileExists "$OBSPath\bin\64bit\obs64.exe" obs_done

    ReadRegStr $0 HKLM \
    "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio" \
    "DisplayIcon"
    StrCpy $1 $0 20 -20
    StrCmp $1 "\bin\64bit\obs64.exe" 0 obs_try_wow
    StrCpy $OBSPath $0 -20
    IfFileExists "$OBSPath\bin\64bit\obs64.exe" obs_done

obs_try_wow:
    ReadRegStr $OBSPath HKLM \
    "SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio" \
    "InstallLocation"
    IfFileExists "$OBSPath\bin\64bit\obs64.exe" obs_done

    ReadRegStr $0 HKLM \
    "SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\OBS Studio" \
    "DisplayIcon"
    StrCpy $1 $0 20 -20
    StrCmp $1 "\bin\64bit\obs64.exe" 0 obs_try_pf64
    StrCpy $OBSPath $0 -20
    IfFileExists "$OBSPath\bin\64bit\obs64.exe" obs_done

obs_try_pf64:
    StrCpy $OBSPath "$PROGRAMFILES64\obs-studio"
    IfFileExists "$OBSPath\bin\64bit\obs64.exe" obs_done

    StrCpy $OBSPath "$PROGRAMFILES\obs-studio"
    IfFileExists "$OBSPath\bin\64bit\obs64.exe" obs_done

    StrCpy $OBSPath "$PROGRAMFILES64\obs-studio"

obs_done:

    ; --- Detect foobar2000 ---
    StrCpy $FB2KPath ""

    ; Try foobar2000 v2 registry key (HKLM Software\foobar2000 InstallDir)
    ReadRegStr $FB2KPath HKLM \
    "Software\foobar2000" "InstallDir"
    IfFileExists "$FB2KPath\foobar2000.exe" fb2k_done

    ReadRegStr $FB2KPath HKCU \
    "Software\foobar2000" "InstallDir"
    IfFileExists "$FB2KPath\foobar2000.exe" fb2k_done

    ; Try foil key (older v1 installs)
    ReadRegStr $FB2KPath HKCU \
    "Software\foobar2000\foil" ""
    IfFileExists "$FB2KPath\foobar2000.exe" fb2k_done

    ReadRegStr $FB2KPath HKLM \
    "Software\foobar2000\foil" ""
    IfFileExists "$FB2KPath\foobar2000.exe" fb2k_done

    ; Try uninstall registry keys
    ReadRegStr $FB2KPath HKLM \
    "Software\Microsoft\Windows\CurrentVersion\Uninstall\foobar2000 (x64)" \
    "InstallLocation"
    IfFileExists "$FB2KPath\foobar2000.exe" fb2k_done

    ReadRegStr $FB2KPath HKCU \
    "Software\Microsoft\Windows\CurrentVersion\Uninstall\foobar2000 (x64)" \
    "InstallLocation"
    IfFileExists "$FB2KPath\foobar2000.exe" fb2k_done

    ReadRegStr $FB2KPath HKLM \
    "Software\Microsoft\Windows\CurrentVersion\Uninstall\foobar2000" \
    "InstallLocation"
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

    ; Not found - leave empty for user to browse
    StrCpy $FB2KPath ""

fb2k_done:

    StrCpy $INSTDIR "$OBSPath"

FunctionEnd

; --- Page 1 leave: save OBS path before page 2 ---
Function obs_page_leave
    StrCpy $OBSPath "$INSTDIR"
FunctionEnd

; --- Page 2: foobar2000 custom browse ---
Function fb2k_page_show

    StrCmp $FB2KPath "" 0 fb2k_has_path
    StrCpy $FB2KPath "$PROGRAMFILES64\foobar2000"
fb2k_has_path:
    StrCpy $FB2KDirText "$FB2KPath"

    nsDialogs::Create 1018
    Pop $0

    ${NSD_CreateLabel} 0u 0u 100% 20u "foobar2000 installation folder:"
    Pop $0

    ${NSD_CreateDirRequest} 0u 20u 194u 12u "$FB2KPath"
    Pop $FB2KDirHWND

    ${NSD_CreateBrowseButton} 197u 20u 62u 13u "Browse"
    Pop $0
    ${NSD_OnClick} $0 fb2k_browse_click

    nsDialogs::Show

FunctionEnd

Function fb2k_browse_click
    nsDialogs::SelectFolderDialog "Select foobar2000 installation folder" "$FB2KPath"
    Pop $FB2KPath
    StrCmp $FB2KPath "" 0 fb2k_browse_check_error
    StrCpy $FB2KPath "$FB2KDirText"
    Goto fb2k_browse_done
fb2k_browse_check_error:
    StrCmp $FB2KPath "error" 0 fb2k_browse_done
    StrCpy $FB2KPath "$FB2KDirText"
fb2k_browse_done:
    ${NSD_SetText} $FB2KDirHWND "$FB2KPath"
FunctionEnd

Function fb2k_page_leave
    ${NSD_GetText} $FB2KDirHWND $FB2KPath
FunctionEnd

Section "Install"

    StrCpy $0 "$OBSPath"
    StrCpy $2 "$FB2KPath"

    IfFileExists "$0\bin\64bit\obs64.exe" obs_ok obs_fail

obs_fail:
    MessageBox MB_ICONSTOP "Invalid OBS folder: $0"
    Abort

obs_ok:

    SetOutPath "$0\obs-plugins\64bit"
    File "build_x64\rundir\RelWithDebInfo\foobar2000-obs.dll"
    File "build_x64\rundir\RelWithDebInfo\foobar2000-obs.pdb"

    SetOutPath "$0\data\obs-plugins\foobar2000-obs\locale"
    File "build_x64\rundir\RelWithDebInfo\foobar2000-obs\locale\en-US.ini"

    IfFileExists "$2\foobar2000.exe" fb2k_ok fb2k_skip

fb2k_skip:
    MessageBox MB_OK "foobar2000 not found in selected folder. Bridge component skipped.$\n$\nCopy foo_obsbridge.dll to your foobar2000 components folder manually."
    Goto done

fb2k_ok:

    StrCpy $3 "$2\components"
    CreateDirectory "$3"
    SetOutPath "$3"
    File "build_x64\out\foo_obsbridge.dll"

    MessageBox MB_OK "foobar2000-obs installed successfully.$\n$\nOBS plugin: $0$\nfoobar2000 bridge: $3$\n$\nRestart both OBS and foobar2000 to activate."

done:

SectionEnd
