; InnoSetup file

[Setup]
AppName=Minimum Profit
AppVerName=Minimum Profit version 5.x
DefaultDirName={pf}\mp-5
UsePreviousAppDir=no
DefaultGroupName=Minimum Profit
UninstallDisplayIcon={app}\mp-5.exe
Compression=lzma
SolidCompression=yes
; OutputDir=userdocs:Inno Setup Examples Output

[Files]
Source: "mp-5.exe"; DestDir: "{app}"
Source: "mp-5c.exe"; DestDir: "{app}"
Source: "doc\*.txt"; DestDir: "{app}\doc"
Source: "README" ; DestDir: "{app}\doc"
Source: "AUTHORS" ; DestDir: "{app}\doc"
Source: "LICENSE" ; DestDir: "{app}\doc"
Source: "RELEASE_NOTES" ; DestDir: "{app}\doc"
Source: "mp_templates.sample" ; DestDir: "{app}\doc"
Source: "TODO" ; DestDir: "{app}\doc"

[Icons]
Name: "{group}\Minimum Profit"; Filename: "{app}\mp-5.exe"

[Registry]
Root: HKCR; Subkey: "*\shell\Open with MP"; ValueType: string;
Root: HKCR; Subkey: "*\shell\Open with MP\command"; ValueType: string; ValueData: "{app}\mp-5.exe %1"
Root: HKCR; Subkey: "*\shell\Hex view with MP"; ValueType: string;
Root: HKCR; Subkey: "*\shell\Hex view with MP\command"; ValueType: string; ValueData: "{app}\mp-5.exe -x %1"
Root: HKCR; Subkey: "Directory\shell\minimum_profit"; ValueType: string; ValueData: "Open MP Here"
Root: HKCR; Subkey: "Directory\shell\minimum_profit\command"; ValueType: string; ValueData: "{app}\mp-5.exe -d %1"
Root: HKLM; Subkey: "Software\mp-5"; ValueType: string;
Root: HKLM; Subkey: "Software\mp-5\appdir"; ValueType: string; ValueData: "{app}"
