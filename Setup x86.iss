[Setup]
AppName=Image Eye
AppVerName=Image Eye v9.1 x86
AppPublisher=FMJ-Software
AppPublisherURL=http://www.fmjsoft.com/
AppSupportURL=http://www.fmjsoft.com/
AppUpdatesURL=http://www.fmjsoft.com/
ChangesAssociations=yes
Compression=lzma2/max
DefaultDirName={pf}\Image Eye
DefaultGroupName=Image Eye
DisableStartupPrompt=yes
DisableProgramGroupPage=yes
OutputDir=Out
UninstallDisplayIcon={app}\Image Eye.exe

[Tasks]
Name: "startmenu"; Description: "Create &group in the Start menu"; GroupDescription: "Program group icons:"; MinVersion: 4,4
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"; MinVersion: 4,4
Name: "quicklaunchicon"; Description: "Create a &Quick Launch icon"; GroupDescription: "Additional icons:"; Flags: unchecked

[Files]
; Install new files
Source: "Out\Image Eye.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "Out\Image Eye.chm"; DestDir: "{app}"; Flags: ignoreversion
Source: "Out\*.language"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
; Program group icons
Name: "{group}\Image Eye"; Filename: "{app}\Image Eye.exe"; Tasks: startmenu
Name: "{group}\Image Eye program manual"; Filename: "{app}\Image Eye.chm"; Tasks: startmenu
Name: "{group}\Image Eye web site"; Filename: "http://www.fmjsoft.com/imageeye.html"; Tasks: startmenu; IconFilename: "{sys}\shell32.dll"; IconIndex: 220
Name: "{group}\Uninstall Image Eye"; Filename: "{app}\unins000.exe"; Tasks: startmenu
; Desktop and taskbar icons
Name: "{userdesktop}\Image Eye"; Filename: "{app}\Image Eye.exe"; MinVersion: 4,4; Tasks: desktopicon;
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\Image Eye"; Filename: "{app}\Image Eye.exe"; Tasks: quicklaunchicon

[Registry]
; Clean up after previous versions deprecated stuff
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\ImageEye.exe"; Flags: deletekey dontcreatekey
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Image Eye"; Flags: deletekey dontcreatekey
Root: HKLM; Subkey: "SOFTWARE\Classes\Directory\\shell\\Image Eye index"; Flags: deletekey dontcreatekey
; Set app-path
Root: HKLM; Subkey: "SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\Image Eye.exe"; ValueType: string; ValueName: ""; ValueData: "{app}\Image Eye.exe"; Flags: uninsdeletekey
; Clean up global settings when uninstalling
Root: HKLM; Subkey: "SOFTWARE\Classes\ImageEye"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software"; Flags: uninsdeletekeyifempty
Root: HKLM; Subkey: "SOFTWARE\Classes\Directory\\shell\\View as Image Eye index"; Flags: uninsdeletekey
; Clean up per-user settings when uninstalling
Root: HKCU; Subkey: "Software\FMJ-Software\Image Eye"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\FMJ-Software"; Flags: uninsdeletekeyifempty
; Register "View as Image Eye index" command for directories
Root: HKLM; Subkey: "SOFTWARE\Classes\Directory\\shell\\View as Image Eye index\\command"; ValueType: string; ValueName: ""; ValueData: "{app}\Image Eye.exe -index ""%1"""
; Register file type association ProgId
Root: HKLM; Subkey: "SOFTWARE\Classes\ImageEye"; ValueType: string; ValueName: ""; ValueData: "Image file"
Root: HKLM; Subkey: "SOFTWARE\Classes\ImageEye\DefaultIcon"; ValueType: expandsz; ValueName: ""; ValueData: "{app}\Image Eye.exe, 0"
Root: HKLM; Subkey: "SOFTWARE\Classes\ImageEye\shell\open"; ValueType: expandsz; ValueName: ""; ValueData: "View with Image Eye"
Root: HKLM; Subkey: "SOFTWARE\Classes\ImageEye\shell\open\command"; ValueType: expandsz; ValueName: ""; ValueData: "{app}\Image Eye.exe ""%1"""
; Register as default-program (Windows Vista/7)
Root: HKLM; Subkey: "SOFTWARE\RegisteredApplications"; ValueType: string; ValueName: "Image Eye"; ValueData: "SOFTWARE\FMJ-Software\Image Eye\Capabilities"; Flags: uninsdeletevalue
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "The fast, free, no-nonsense image viewer"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "Image Eye"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".bmp"; ValueData: "ImageEye"
;Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".cur"; ValueData: "ImageEye"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".dds"; ValueData: "ImageEye"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".dib"; ValueData: "ImageEye"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".fit"; ValueData: "ImageEye"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".fits"; ValueData: "ImageEye"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".gif"; ValueData: "ImageEye"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".hdr"; ValueData: "ImageEye"
;Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ico"; ValueData: "ImageEye"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".ies"; ValueData: "ImageEye"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".iff"; ValueData: "ImageEye"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".img"; ValueData: "ImageEye"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".jif"; ValueData: "ImageEye"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".jpe"; ValueData: "ImageEye"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".jpeg"; ValueData: "ImageEye"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".jpg"; ValueData: "ImageEye"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".pcx"; ValueData: "ImageEye"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".pic"; ValueData: "ImageEye"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".png"; ValueData: "ImageEye"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".psd"; ValueData: "ImageEye"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".raw"; ValueData: "ImageEye"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".rle"; ValueData: "ImageEye"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tga"; ValueData: "ImageEye"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tif"; ValueData: "ImageEye"
Root: HKLM; Subkey: "SOFTWARE\FMJ-Software\Image Eye\Capabilities\FileAssociations"; ValueType: string; ValueName: ".tiff"; ValueData: "ImageEye"

[Run]
Filename: "{app}\Image Eye.exe"; Description: "Launch Image Eye"; Flags: nowait postinstall skipifsilent


