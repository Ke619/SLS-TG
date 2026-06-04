[Setup]
AppName=SLS Ticket Grabber
AppVersion=2026.06.04.rv1.1
AppPublisher=Ke619
AppPublisherURL=https://github.com/Ke619/SLS-TG
DefaultDirName={autopf}\SLS-TG
DefaultGroupName=SLS Ticket Grabber
OutputDir=.
OutputBaseFilename=SLS-TG-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupIconFile=Icon.ico
UninstallDisplayIcon={app}\SLS-TG.exe

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Files]
Source: "*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs; Excludes: "installer.iss,Icon.ico"

[Icons]
Name: "{group}\SLS Ticket Grabber"; Filename: "{app}\SLS-TG.exe"
Name: "{group}\Uninstall SLS Ticket Grabber"; Filename: "{uninstallexe}"
Name: "{commondesktop}\SLS Ticket Grabber"; Filename: "{app}\SLS-TG.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\SLS-TG.exe"; Description: "{cm:LaunchProgram,SLS Ticket Grabber}"; Flags: nowait postinstall skipifsilent
