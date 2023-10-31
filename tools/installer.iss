; used in build-installer.ps1
; if set, must end in a backslash
#ifndef WORKING_DIR
#define WORKING_DIR ""
#endif

[Setup]
AppName=Stickit
AppVersion=0.1.0
AppPublisher=Nerixyz
AppId={{19EEBCE6-B42E-4F64-B744-A197995F7A15}

WizardStyle=modern

DefaultDirName={autopf}\Stickit
DefaultGroupName=Stickit
UninstallDisplayIcon={app}\Stickit.exe

Compression=lzma2
SolidCompression=yes

ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64

OutputBaseFilename=Stickit.Installer

[Files]
Source: "{#WORKING_DIR}bin\Stickit.exe"; DestDir: "{app}"; DestName: "Stickit.exe"

[Icons]
Name: "{group}\Stickit"; Filename: "{app}\Stickit.exe"
