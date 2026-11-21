; ============================================================================
; ToriGraph Windows All-in-One Installer (App + OLE Server + Python SDK)
; Built with Inno Setup 6 (Unicode)
; ============================================================================

#ifndef MyAppVersion
  #define MyAppVersion "2.4.0"
#endif

#ifndef StageDir
  #define StageDir "..\dist\ToriGraph-win64"
#endif

#ifndef OutDir
  #define OutDir "..\dist"
#endif

#define MyAppName "ToriGraph"
#define MyAppPublisher "ToriGraph Project"
#define MyAppURL "https://github.com/kzykshmz/ToriGraph"
#define MyAppExeName "ToriGraph.exe"
#define MyOleClsid "{8E6B9541-3C74-4D28-A915-7D84E5A12001}"
#define MyOleProgId "ToriGraph.Graph.1"
#define MyLegacyProgId "SciDAVis.Graph.1"

[Setup]
AppId={{8E6B9541-3C74-4D28-A915-7D84E5A12001}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}/issues
AppUpdatesURL={#MyAppURL}/releases
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
LicenseFile=..\LICENSE
OutputDir={#OutDir}
OutputBaseFilename=ToriGraph-{#MyAppVersion}-win64-setup
SetupIconFile=..\scidavis\icons\scidavis.ico
UninstallDisplayIcon={app}\{#MyAppExeName}
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ChangesEnvironment=yes
ChangesAssociations=yes

[Languages]
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
japanese.TaskGroupIntegration=統合機能 (OLE サーバー / Python SDK / 環境設定):
english.TaskGroupIntegration=Integration (OLE Server / Python SDK / Environment):
japanese.TaskRegisterOle=PowerPoint / Word 用 OLE サーバーを登録する (埋め込みグラフのダブルクリック再編集)
english.TaskRegisterOle=Register OLE Server for PowerPoint / Word (in-place double-click graph editing)
japanese.TaskInstallPython=Python 自動化 SDK (torigraph) を検出された Python 環境へ自動インストールする
english.TaskInstallPython=Install torigraph Python automation SDK into detected Python environment
japanese.TaskEnvPath=環境変数 TORIGRAPH_EXE およびユーザー PATH に ToriGraph を追加する
english.TaskEnvPath=Set TORIGRAPH_EXE environment variable and add ToriGraph to user PATH
japanese.TaskAssocProject=プロジェクトファイル (.sciprj, .sciprj.gz) を ToriGraph に関連付ける
english.TaskAssocProject=Associate project files (.sciprj, .sciprj.gz) with ToriGraph
japanese.MenuRepairOle=OLE サーバーを再登録 (PowerPoint 連携修復)
english.MenuRepairOle=Re-register OLE Server (Repair PowerPoint Integration)
japanese.MenuInstallPySdk=Python SDK (torigraph) を再インストール
english.MenuInstallPySdk=Re-install Python SDK (torigraph)
japanese.StatusInstallPython=Python 自動化 SDK (torigraph) をインストールしています...
english.StatusInstallPython=Installing torigraph Python automation SDK...
japanese.LaunchAfterInstall=ToriGraph を今すぐ起動する
english.LaunchAfterInstall=Launch ToriGraph now

[Tasks]
Name: "registerole"; Description: "{cm:TaskRegisterOle}"; GroupDescription: "{cm:TaskGroupIntegration}"
Name: "installpython"; Description: "{cm:TaskInstallPython}"; GroupDescription: "{cm:TaskGroupIntegration}"
Name: "addtopath"; Description: "{cm:TaskEnvPath}"; GroupDescription: "{cm:TaskGroupIntegration}"
Name: "assocsciprj"; Description: "{cm:TaskAssocProject}"; GroupDescription: "{cm:TaskGroupIntegration}"
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:MenuRepairOle}"; Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\scripts\register-ole.ps1"" -Mode Install -ExePath ""{app}\{#MyAppExeName}"""; WorkingDir: "{app}"
Name: "{group}\{cm:MenuInstallPySdk}"; Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\scripts\install-python-sdk.ps1"" -SdkDir ""{app}\python"""; WorkingDir: "{app}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; --- OLE Server Registration (HKCU, matches libscidavis/src/OleServer.cpp) ---
Root: HKCU; Subkey: "Software\Classes\CLSID\{#MyOleClsid}"; ValueType: string; ValueName: ""; ValueData: "ToriGraph Graph"; Flags: uninsdeletekey; Tasks: registerole
Root: HKCU; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\LocalServer32"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" -Embedding"; Tasks: registerole
Root: HKCU; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\LocalServer32"; ValueType: string; ValueName: "ServerExecutable"; ValueData: "{app}\{#MyAppExeName}"; Tasks: registerole
Root: HKCU; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\ProgID"; ValueType: string; ValueName: ""; ValueData: "{#MyOleProgId}"; Tasks: registerole
Root: HKCU; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\VersionIndependentProgID"; ValueType: string; ValueName: ""; ValueData: "ToriGraph.Graph"; Tasks: registerole
Root: HKCU; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\InprocHandler32"; ValueType: string; ValueName: ""; ValueData: "ole32.dll"; Tasks: registerole
Root: HKCU; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"",0"; Tasks: registerole
Root: HKCU; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\Verb\0"; ValueType: string; ValueName: ""; ValueData: "編集(&E),0,2"; Tasks: registerole
Root: HKCU; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\Verb\1"; ValueType: string; ValueName: ""; ValueData: "開く(&O),0,2"; Tasks: registerole
Root: HKCU; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\DataFormats\GetSet\0"; ValueType: string; ValueName: ""; ValueData: "Embed Source,1,8,1"; Tasks: registerole
Root: HKCU; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\DataFormats\GetSet\1"; ValueType: string; ValueName: ""; ValueData: "Embedded Object,1,8,1"; Tasks: registerole
Root: HKCU; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\DataFormats\GetSet\2"; ValueType: string; ValueName: ""; ValueData: "Object Descriptor,1,1,1"; Tasks: registerole
Root: HKCU; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\DataFormats\GetSet\3"; ValueType: string; ValueName: ""; ValueData: "14,1,64,1"; Tasks: registerole
Root: HKCU; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\MiscStatus"; ValueType: string; ValueName: ""; ValueData: "0"; Tasks: registerole
Root: HKCU; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\MiscStatus\1"; ValueType: string; ValueName: ""; ValueData: "1"; Tasks: registerole
Root: HKCU; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\AuxUserType\2"; ValueType: string; ValueName: ""; ValueData: "ToriGraph"; Tasks: registerole
Root: HKCU; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\AuxUserType\3"; ValueType: string; ValueName: ""; ValueData: "ToriGraph"; Tasks: registerole
Root: HKCU; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\Insertable"; ValueType: string; ValueName: ""; ValueData: ""; Tasks: registerole

Root: HKCU; Subkey: "Software\Classes\{#MyOleProgId}"; ValueType: string; ValueName: ""; ValueData: "ToriGraph Graph"; Flags: uninsdeletekey; Tasks: registerole
Root: HKCU; Subkey: "Software\Classes\{#MyOleProgId}\CLSID"; ValueType: string; ValueName: ""; ValueData: "{#MyOleClsid}"; Tasks: registerole
Root: HKCU; Subkey: "Software\Classes\{#MyOleProgId}\Insertable"; ValueType: string; ValueName: ""; ValueData: ""; Tasks: registerole
Root: HKCU; Subkey: "Software\Classes\{#MyOleProgId}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: registerole

Root: HKCU; Subkey: "Software\Classes\{#MyLegacyProgId}"; ValueType: string; ValueName: ""; ValueData: "ToriGraph Graph"; Flags: uninsdeletekey; Tasks: registerole
Root: HKCU; Subkey: "Software\Classes\{#MyLegacyProgId}\CLSID"; ValueType: string; ValueName: ""; ValueData: "{#MyOleClsid}"; Tasks: registerole
Root: HKCU; Subkey: "Software\Classes\{#MyLegacyProgId}\Insertable"; ValueType: string; ValueName: ""; ValueData: ""; Tasks: registerole
Root: HKCU; Subkey: "Software\Classes\{#MyLegacyProgId}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: registerole

; --- File Associations (.sciprj / .sciprj.gz) ---
Root: HKCU; Subkey: "Software\Classes\.sciprj"; ValueType: string; ValueName: ""; ValueData: "ToriGraph.Project"; Flags: uninsdeletevalue; Tasks: assocsciprj
Root: HKCU; Subkey: "Software\Classes\ToriGraph.Project"; ValueType: string; ValueName: ""; ValueData: "ToriGraph Project File"; Flags: uninsdeletekey; Tasks: assocsciprj
Root: HKCU; Subkey: "Software\Classes\ToriGraph.Project\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"",0"; Tasks: assocsciprj
Root: HKCU; Subkey: "Software\Classes\ToriGraph.Project\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: assocsciprj

; --- Environment variable TORIGRAPH_EXE ---
Root: HKCU; Subkey: "Environment"; ValueType: string; ValueName: "TORIGRAPH_EXE"; ValueData: "{app}\{#MyAppExeName}"; Flags: uninsdeletevalue; Tasks: addtopath

[Run]
Filename: "powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File ""{app}\scripts\install-python-sdk.ps1"" -SdkDir ""{app}\python"" -Silent"; StatusMsg: "{cm:StatusInstallPython}"; Flags: runhidden waituntilterminated; Tasks: installpython
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchAfterInstall}"; Flags: nowait postinstall skipifsilent

[Code]
const
  EnvSubKey = 'Environment';

function NeedsAddPath(Param: string): Boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER, EnvSubKey, 'Path', OrigPath) then
  begin
    Result := True;
    exit;
  end;
  Result := Pos(';' + UpperCase(Param) + ';', ';' + UpperCase(OrigPath) + ';') = 0;
end;

procedure AddDirToUserPath(DirPath: string);
var
  OrigPath: string;
begin
  if not NeedsAddPath(DirPath) then
    exit;
  if not RegQueryStringValue(HKEY_CURRENT_USER, EnvSubKey, 'Path', OrigPath) then
    OrigPath := '';
  if (OrigPath <> '') and (OrigPath[Length(OrigPath)] <> ';') then
    OrigPath := OrigPath + ';';
  OrigPath := OrigPath + DirPath;
  RegWriteExpandStringValue(HKEY_CURRENT_USER, EnvSubKey, 'Path', OrigPath);
end;

procedure RemoveDirFromUserPath(DirPath: string);
var
  OrigPath, UpperPath, UpperDir, NewPath: string;
  P: Integer;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER, EnvSubKey, 'Path', OrigPath) then
    exit;
  UpperPath := ';' + UpperCase(OrigPath) + ';';
  UpperDir := ';' + UpperCase(DirPath) + ';';
  P := Pos(UpperDir, UpperPath);
  if P = 0 then
    exit;
  NewPath := Copy(OrigPath, 1, P - 1) + Copy(OrigPath, P + Length(DirPath) + 1, MaxInt);
  while (Length(NewPath) > 0) and (NewPath[1] = ';') do
    Delete(NewPath, 1, 1);
  while (Length(NewPath) > 0) and (NewPath[Length(NewPath)] = ';') do
    Delete(NewPath, Length(NewPath), 1);
  StringChangeEx(NewPath, ';;', ';', True);
  RegWriteExpandStringValue(HKEY_CURRENT_USER, EnvSubKey, 'Path', NewPath);
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and WizardIsTaskSelected('addtopath') then
  begin
    AddDirToUserPath(ExpandConstant('{app}'));
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
  begin
    RemoveDirFromUserPath(ExpandConstant('{app}'));
  end;
end;
