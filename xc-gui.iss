#define MyAppName "XC Gui"
#define TargetName "xc-gui"

[Setup]
AppName={#MyAppName}
AppVersion=0.8.6
DefaultDirName={autopf}\AHP\{#MyAppName}
DefaultGroupName=AHP
UninstallDisplayIcon={app}\{#TargetName}.exe
WizardStyle=modern
Compression=lzma2
SolidCompression=yes
OutputDir="./"
ArchitecturesInstallIn64BitMode=x64
OutputBaseFilename={#MyAppName}_Setup
SetupIconFile=icon.ico

[Files] 
Source: "../bin/{#TargetName}64/*"; DestDir: "{app}"; Check: Is64BitInstallMode ; Flags: solidbreak recursesubdirs
Source: "../bin/{#TargetName}32/*"; DestDir: "{app}"; Check: not Is64BitInstallMode; Flags: solidbreak recursesubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#TargetName}.exe"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#TargetName}.exe"