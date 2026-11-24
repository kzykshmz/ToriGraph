; ============================================================================
; ToriGraph Windows All-in-One Installer (App + OLE Server + Python SDK)
; Built with Inno Setup 6 (Unicode)
; ============================================================================

; The version is passed in by scripts/build-windows-installer.ps1, which reads it
; from the top-level CMakeLists.txt project() VERSION (single source of truth).
#ifndef MyAppVersion
  #error MyAppVersion is not defined. Build the installer via scripts\build-windows-installer.ps1.
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
#define MyOleClsid "{{8E6B9541-3C74-4D28-A915-7D84E5A12001}"
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
; Binary distributions are GPL-3.0-or-later (sources: GPL-2.0-or-later, see LICENSE / THIRD_PARTY_LICENSES.md)
LicenseFile=..\LICENSE-GPL-3.0.txt
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
japanese.TaskEnvPath=環境変数 TORIGRAPH_EXE と PATH に ToriGraph を追加する (全ユーザー用インストールではシステム PATH)
english.TaskEnvPath=Set TORIGRAPH_EXE environment variable and add ToriGraph to PATH (system PATH for an all-users install)
japanese.TaskAssocProject=プロジェクトファイル (.sciprj) を ToriGraph に関連付ける
english.TaskAssocProject=Associate project files (.sciprj) with ToriGraph
japanese.MenuRepairOle=OLE サーバーを再登録 (PowerPoint 連携修復)
english.MenuRepairOle=Re-register OLE Server (Repair PowerPoint Integration)
japanese.MenuQuickGuide=ToriGraph かんたんガイド (使い方・FAQ)
english.MenuQuickGuide=ToriGraph Quick Guide (Japanese)
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
; F1 quick guide (manual\index.html). Already staged by build-windows-installer.ps1; listed explicitly so a
; stage without the manual fails loudly instead of shipping an installer whose F1 falls back to the web.
Source: "{#StageDir}\manual\index.html"; DestDir: "{app}\manual"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:MenuRepairOle}"; Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{app}\scripts\register-ole.ps1"" -Mode Install -ExePath ""{app}\{#MyAppExeName}"" -PauseOnError"; WorkingDir: "{app}"
Name: "{group}\{cm:MenuQuickGuide}"; Filename: "{app}\manual\index.html"
Name: "{group}\{cm:MenuInstallPySdk}"; Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -Command ""try {{ & \""{app}\scripts\install-python-sdk.ps1\"" -SdkDir \""{app}\python\""; if ($LASTEXITCODE -and $LASTEXITCODE -ne 0) {{ throw 'Exit code ' + $LASTEXITCODE }} }} catch {{ Write-Host $_ -ForegroundColor Red; [void](Read-Host 'Press Enter to close') }}"""; WorkingDir: "{app}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Registry]
; --- OLE Server Registration (HKA: HKCU per-user / HKLM all-users, matches libscidavis/src/OleServer.cpp) ---
Root: HKA; Subkey: "Software\Classes\CLSID\{#MyOleClsid}"; ValueType: string; ValueName: ""; ValueData: "ToriGraph Graph"; Flags: uninsdeletekey; Tasks: registerole
Root: HKA; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\LocalServer32"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" -Embedding"; Tasks: registerole
Root: HKA; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\LocalServer32"; ValueType: string; ValueName: "ServerExecutable"; ValueData: "{app}\{#MyAppExeName}"; Tasks: registerole
Root: HKA; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\ProgID"; ValueType: string; ValueName: ""; ValueData: "{#MyOleProgId}"; Tasks: registerole
Root: HKA; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\VersionIndependentProgID"; ValueType: string; ValueName: ""; ValueData: "ToriGraph.Graph"; Tasks: registerole
Root: HKA; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\InprocHandler32"; ValueType: string; ValueName: ""; ValueData: "ole32.dll"; Tasks: registerole
Root: HKA; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"",0"; Tasks: registerole
Root: HKA; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\Verb\0"; ValueType: string; ValueName: ""; ValueData: "編集(&E),0,2"; Tasks: registerole
Root: HKA; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\Verb\1"; ValueType: string; ValueName: ""; ValueData: "開く(&O),0,2"; Tasks: registerole
Root: HKA; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\DataFormats\GetSet\0"; ValueType: string; ValueName: ""; ValueData: "Embed Source,1,8,1"; Tasks: registerole
Root: HKA; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\DataFormats\GetSet\1"; ValueType: string; ValueName: ""; ValueData: "Embedded Object,1,8,1"; Tasks: registerole
Root: HKA; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\DataFormats\GetSet\2"; ValueType: string; ValueName: ""; ValueData: "Object Descriptor,1,1,1"; Tasks: registerole
Root: HKA; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\DataFormats\GetSet\3"; ValueType: string; ValueName: ""; ValueData: "14,1,64,1"; Tasks: registerole
Root: HKA; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\MiscStatus"; ValueType: string; ValueName: ""; ValueData: "0"; Tasks: registerole
Root: HKA; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\MiscStatus\1"; ValueType: string; ValueName: ""; ValueData: "1"; Tasks: registerole
Root: HKA; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\AuxUserType\2"; ValueType: string; ValueName: ""; ValueData: "ToriGraph"; Tasks: registerole
Root: HKA; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\AuxUserType\3"; ValueType: string; ValueName: ""; ValueData: "ToriGraph"; Tasks: registerole
Root: HKA; Subkey: "Software\Classes\CLSID\{#MyOleClsid}\Insertable"; ValueType: string; ValueName: ""; ValueData: ""; Tasks: registerole

Root: HKA; Subkey: "Software\Classes\{#MyOleProgId}"; ValueType: string; ValueName: ""; ValueData: "ToriGraph Graph"; Flags: uninsdeletekey; Tasks: registerole
Root: HKA; Subkey: "Software\Classes\{#MyOleProgId}\CLSID"; ValueType: string; ValueName: ""; ValueData: "{#MyOleClsid}"; Tasks: registerole
Root: HKA; Subkey: "Software\Classes\{#MyOleProgId}\Insertable"; ValueType: string; ValueName: ""; ValueData: ""; Tasks: registerole
Root: HKA; Subkey: "Software\Classes\{#MyOleProgId}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: registerole

Root: HKA; Subkey: "Software\Classes\{#MyLegacyProgId}"; ValueType: string; ValueName: ""; ValueData: "ToriGraph Graph"; Flags: uninsdeletekey; Tasks: registerole
Root: HKA; Subkey: "Software\Classes\{#MyLegacyProgId}\CLSID"; ValueType: string; ValueName: ""; ValueData: "{#MyOleClsid}"; Tasks: registerole
Root: HKA; Subkey: "Software\Classes\{#MyLegacyProgId}\Insertable"; ValueType: string; ValueName: ""; ValueData: ""; Tasks: registerole
Root: HKA; Subkey: "Software\Classes\{#MyLegacyProgId}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: registerole

; --- File Associations (.sciprj / .sciprj.gz) ---
Root: HKA; Subkey: "Software\Classes\.sciprj"; ValueType: string; ValueName: ""; ValueData: "ToriGraph.Project"; Flags: uninsdeletevalue; Tasks: assocsciprj
Root: HKA; Subkey: "Software\Classes\ToriGraph.Project"; ValueType: string; ValueName: ""; ValueData: "ToriGraph Project File"; Flags: uninsdeletekey; Tasks: assocsciprj
Root: HKA; Subkey: "Software\Classes\ToriGraph.Project\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"",0"; Tasks: assocsciprj
Root: HKA; Subkey: "Software\Classes\ToriGraph.Project\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: assocsciprj

; --- Environment variable TORIGRAPH_EXE ---
Root: HKA; Subkey: "Environment"; ValueType: string; ValueName: "TORIGRAPH_EXE"; ValueData: "{app}\{#MyAppExeName}"; Flags: uninsdeletevalue; Tasks: addtopath; Check: not IsAdminInstallMode
Root: HKA; Subkey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; ValueType: string; ValueName: "TORIGRAPH_EXE"; ValueData: "{app}\{#MyAppExeName}"; Flags: uninsdeletevalue; Tasks: addtopath; Check: IsAdminInstallMode

[Run]
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File ""{app}\scripts\install-python-sdk.ps1"" -SdkDir ""{app}\python"" -Silent"; StatusMsg: "{cm:StatusInstallPython}"; Flags: runhidden waituntilterminated runasoriginaluser; Tasks: installpython
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchAfterInstall}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
; Runs before the files are removed; best-effort (exit 0), never blocks the uninstall. HKCU/HKLM OLE keys are
; removed by the [Registry] uninsdeletekey flags and CleanupStaleHkcuOleRegistration below.
Filename: "{sys}\WindowsPowerShell\v1.0\powershell.exe"; Parameters: "-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File ""{app}\scripts\install-python-sdk.ps1"" -Uninstall -Silent"; RunOnceId: "UninstallPythonSdk"; Flags: runhidden waituntilterminated skipifdoesntexist; Tasks: installpython

[UninstallDelete]
; Files created after installation (pip build output, byte-code caches) that Inno does not track.
Type: filesandordirs; Name: "{app}\python\build"
Type: filesandordirs; Name: "{app}\python\torigraph.egg-info"
Type: filesandordirs; Name: "{app}\python\torigraph\__pycache__"
Type: filesandordirs; Name: "{app}\scripts\__pycache__"
Type: filesandordirs; Name: "{localappdata}\ToriGraph\UpdateStaging"
Type: dirifempty; Name: "{app}\python"
Type: dirifempty; Name: "{app}"

[Code]
const
  EnvSubKey = 'Environment';
  OleClsid = '{8E6B9541-3C74-4D28-A915-7D84E5A12001}';
  OleProgId = 'ToriGraph.Graph.1';
  LegacyProgId = 'SciDAVis.Graph.1';
  ProjectProgId = 'ToriGraph.Project';

function ExtractExeFromCommand(Cmd: string): string;
var
  C, Rest: string;
  EndQuote, ExePos, SpacePos: Integer;
begin
  C := Trim(Cmd);
  if C = '' then
  begin
    Result := '';
    exit;
  end;
  if C[1] = '"' then
  begin
    Rest := Copy(C, 2, MaxInt);
    EndQuote := Pos('"', Rest);
    if EndQuote > 0 then
      Result := Trim(Copy(Rest, 1, EndQuote - 1))
    else
      Result := Trim(Rest);
    exit;
  end;
  ExePos := Pos('.EXE', UpperCase(C));
  if ExePos > 0 then
  begin
    Result := Trim(Copy(C, 1, ExePos + 3));
    exit;
  end;
  SpacePos := Pos(' ', C);
  if SpacePos > 0 then
    Result := Trim(Copy(C, 1, SpacePos - 1))
  else
    Result := C;
end;

procedure CleanupStaleHkcuProgId(ProgIdName: string; ExpectedExe: string);
var
  ProgKey, CmdVal, ExePath: string;
begin
  ProgKey := 'Software\Classes\' + ProgIdName;
  if not RegKeyExists(HKEY_CURRENT_USER, ProgKey) then
    exit;
  ExePath := '';
  if RegQueryStringValue(HKEY_CURRENT_USER, ProgKey + '\shell\open\command', '', CmdVal) then
    ExePath := ExtractExeFromCommand(CmdVal);
  if (ExePath <> '') and
     ((not FileExists(ExePath)) or
      ((ExpectedExe <> '') and (CompareText(ExePath, ExpectedExe) <> 0))) then
    RegDeleteKeyIncludingSubkeys(HKEY_CURRENT_USER, ProgKey);
end;

procedure CleanupStaleHkcuOleRegistration(ExpectedOleExe: string; ExpectedProjExe: string);
var
  ClsidKey, LocalServerKey, ValStr, ExePath, AssocVal: string;
  ClsidRemoved: Boolean;
begin
  ClsidRemoved := False;
  ClsidKey := 'Software\Classes\CLSID\' + OleClsid;
  LocalServerKey := ClsidKey + '\LocalServer32';
  if RegKeyExists(HKEY_CURRENT_USER, ClsidKey) then
  begin
    ExePath := '';
    if RegQueryStringValue(HKEY_CURRENT_USER, LocalServerKey, 'ServerExecutable', ValStr) then
      ExePath := ExtractExeFromCommand(ValStr);
    if (ExePath = '') and RegQueryStringValue(HKEY_CURRENT_USER, LocalServerKey, '', ValStr) then
      ExePath := ExtractExeFromCommand(ValStr);
    if (ExePath = '') or (not FileExists(ExePath)) or
       ((ExpectedOleExe <> '') and (CompareText(ExePath, ExpectedOleExe) <> 0)) then
    begin
      RegDeleteKeyIncludingSubkeys(HKEY_CURRENT_USER, ClsidKey);
      ClsidRemoved := True;
    end;
  end;

  CleanupStaleHkcuProgId(OleProgId, ExpectedOleExe);
  CleanupStaleHkcuProgId(LegacyProgId, ExpectedOleExe);
  CleanupStaleHkcuProgId(ProjectProgId, ExpectedProjExe);

  if ClsidRemoved then
  begin
    if RegKeyExists(HKEY_CURRENT_USER, 'Software\Classes\' + OleProgId) and
       (not RegKeyExists(HKEY_CURRENT_USER, 'Software\Classes\' + OleProgId + '\shell\open\command')) then
      RegDeleteKeyIncludingSubkeys(HKEY_CURRENT_USER, 'Software\Classes\' + OleProgId);
    if RegKeyExists(HKEY_CURRENT_USER, 'Software\Classes\' + LegacyProgId) and
       (not RegKeyExists(HKEY_CURRENT_USER, 'Software\Classes\' + LegacyProgId + '\shell\open\command')) then
      RegDeleteKeyIncludingSubkeys(HKEY_CURRENT_USER, 'Software\Classes\' + LegacyProgId);
  end;

  if (not RegKeyExists(HKEY_CURRENT_USER, 'Software\Classes\' + ProjectProgId)) and
     (not RegKeyExists(HKEY_LOCAL_MACHINE, 'Software\Classes\' + ProjectProgId)) and
     RegQueryStringValue(HKEY_CURRENT_USER, 'Software\Classes\.sciprj', '', AssocVal) then
  begin
    if AssocVal = ProjectProgId then
      RegDeleteValue(HKEY_CURRENT_USER, 'Software\Classes\.sciprj', '');
  end;
end;

{ An all-users (admin) install must put ToriGraph on the machine PATH, otherwise the
  standard users who actually run it never see it; a per-user install uses HKCU. }
function PathRootKey(): Integer;
begin
  if IsAdminInstallMode then
    Result := HKEY_LOCAL_MACHINE
  else
    Result := HKEY_CURRENT_USER;
end;

function PathSubKey(): string;
begin
  if IsAdminInstallMode then
    Result := 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment'
  else
    Result := EnvSubKey;
end;

function PathContainsDir(RootKey: Integer; SubKey, DirPath: string): Boolean;
var
  OrigPath: string;
begin
  Result := False;
  if not RegQueryStringValue(RootKey, SubKey, 'Path', OrigPath) then
    exit;
  Result := Pos(';' + UpperCase(DirPath) + ';', ';' + UpperCase(OrigPath) + ';') > 0;
end;

function NeedsAddPath(Param: string): Boolean;
begin
  Result := not PathContainsDir(PathRootKey(), PathSubKey(), Param);
end;

procedure AddDirToPath(DirPath: string);
var
  OrigPath: string;
begin
  if not NeedsAddPath(DirPath) then
    exit;
  if not RegQueryStringValue(PathRootKey(), PathSubKey(), 'Path', OrigPath) then
    OrigPath := '';
  if (OrigPath <> '') and (OrigPath[Length(OrigPath)] <> ';') then
    OrigPath := OrigPath + ';';
  OrigPath := OrigPath + DirPath;
  RegWriteExpandStringValue(PathRootKey(), PathSubKey(), 'Path', OrigPath);
end;

procedure RemoveDirFromPathAt(RootKey: Integer; SubKey, DirPath: string);
var
  OrigPath, UpperPath, UpperDir, NewPath: string;
  P: Integer;
begin
  if not RegQueryStringValue(RootKey, SubKey, 'Path', OrigPath) then
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
  RegWriteExpandStringValue(RootKey, SubKey, 'Path', NewPath);
end;

procedure RemoveDirFromPath(DirPath: string);
begin
  { the current mode's PATH, plus a leftover per-user entry from an earlier per-user install }
  RemoveDirFromPathAt(PathRootKey(), PathSubKey(), DirPath);
  if IsAdminInstallMode then
    RemoveDirFromPathAt(HKEY_CURRENT_USER, EnvSubKey, DirPath);
end;

function EscapePsSingle(S: string): string;
begin
  Result := S;
  StringChangeEx(Result, '''', '''''', True);
end;

procedure CleanupOriginalUserStaleHkcuOle(ExpectedOleExe: string; ExpectedProjExe: string);
var
  PsExe, PsCmd, Params: string;
  ResultCode: Integer;
begin
  PsExe := ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe');
  if not FileExists(PsExe) then
  begin
    Log('CleanupOriginalUserStaleHkcuOle: powershell.exe not found at ' + PsExe);
    exit;
  end;
  PsCmd :=
    '$ErrorActionPreference = ''SilentlyContinue''; ' +
    '$clsid = ''HKCU:\Software\Classes\CLSID\' + OleClsid + '''; ' +
    '$expOle = ''' + EscapePsSingle(ExpectedOleExe) + '''; ' +
    '$expProj = ''' + EscapePsSingle(ExpectedProjExe) + '''; ' +
    'function Get-CmdExe($s) { ' +
    '  $t = if ($null -ne $s) { ('''' + $s) -replace ''^\s+|\s+$'', '''' } else { '''' }; ' +
    '  if (-not $t) { return '''' }; ' +
    '  if ($t -match ''^\x22([^\x22]*)'') { return ($Matches[1] -replace ''^\s+|\s+$'', '''') }; ' +
    '  if ($t -match ''^(.+?\.exe)\b'') { return ($Matches[1] -replace ''^\s+|\s+$'', '''') }; ' +
    '  return (($t -split ''\s+'')[0] -replace ''^\s+|\s+$'', ''''); ' +
    '}; ' +
    'function Test-ExeDiffers($a, $b) { ' +
    '  if (-not $b) { return $false }; ' +
    '  try { return ([System.IO.Path]::GetFullPath($a) -ine [System.IO.Path]::GetFullPath($b)) } ' +
    '  catch { return ($a -ine $b) } ' +
    '}; ' +
    '$removed = $false; ' +
    'if (Test-Path -LiteralPath $clsid) { ' +
    '  $ls = Join-Path $clsid ''LocalServer32''; $exe = ''''; ' +
    '  if (Test-Path -LiteralPath $ls) { ' +
    '    $exe = Get-CmdExe (Get-ItemProperty -LiteralPath $ls -Name ''ServerExecutable'' -ErrorAction SilentlyContinue).ServerExecutable; ' +
    '    if (-not $exe) { $exe = Get-CmdExe (Get-ItemProperty -LiteralPath $ls -Name ''(default)'' -ErrorAction SilentlyContinue).''(default)'' } ' +
    '  }; ' +
    '  if ((-not $exe) -or (-not (Test-Path -LiteralPath $exe -PathType Leaf -ErrorAction SilentlyContinue)) -or (Test-ExeDiffers $exe $expOle)) { Remove-Item -LiteralPath $clsid -Recurse -Force -ErrorAction SilentlyContinue; $removed = $true } ' +
    '}; ' +
    'foreach ($p in @(''' + OleProgId + ''', ''' + LegacyProgId + ''', ''' + ProjectProgId + ''')) { ' +
    '  $pk = ''HKCU:\Software\Classes\'' + $p; ' +
    '  $expTarget = if ($p -eq ''' + ProjectProgId + ''') { $expProj } else { $expOle }; ' +
    '  if (Test-Path -LiteralPath $pk) { ' +
    '    $ck = Join-Path $pk ''shell\open\command''; ' +
    '    if (Test-Path -LiteralPath $ck) { ' +
    '      $ce = Get-CmdExe (Get-ItemProperty -LiteralPath $ck -Name ''(default)'' -ErrorAction SilentlyContinue).''(default)''; ' +
    '      if ($ce -and ((-not (Test-Path -LiteralPath $ce -PathType Leaf -ErrorAction SilentlyContinue)) -or (Test-ExeDiffers $ce $expTarget))) { Remove-Item -LiteralPath $pk -Recurse -Force -ErrorAction SilentlyContinue } ' +
    '    } ' +
    '    elseif ($removed -and ($p -ne ''' + ProjectProgId + ''')) { Remove-Item -LiteralPath $pk -Recurse -Force -ErrorAction SilentlyContinue } ' +
    '  } ' +
    '}; ' +
    '$ext = ''HKCU:\Software\Classes\.sciprj''; ' +
    'if ((-not (Test-Path -LiteralPath ''HKCU:\Software\Classes\' + ProjectProgId + ''')) -and (-not (Test-Path -LiteralPath ''HKLM:\Software\Classes\' + ProjectProgId + ''')) -and (Test-Path -LiteralPath $ext)) { ' +
    '  if (('''' + (Get-ItemProperty -LiteralPath $ext -Name ''(default)'' -ErrorAction SilentlyContinue).''(default)'') -eq ''' + ProjectProgId + ''') { ' +
    '    $ek = Get-Item -LiteralPath $ext -ErrorAction SilentlyContinue; ' +
    '    if ($ek -and (@($ek.Property).Count -le 1) -and (@(Get-ChildItem -LiteralPath $ext -ErrorAction SilentlyContinue).Count -eq 0)) { Remove-Item -LiteralPath $ext -Force -ErrorAction SilentlyContinue } ' +
    '    else { Clear-ItemProperty -LiteralPath $ext -Name ''(default)'' -ErrorAction SilentlyContinue } ' +
    '  } ' +
    '}';
  Params := '-NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -Command "' + PsCmd + '"';
  ResultCode := -1;
  try
    if ExecAsOriginalUser(PsExe, Params, '', SW_HIDE, ewWaitUntilTerminated, ResultCode) then
      Log('CleanupOriginalUserStaleHkcuOle succeeded (ResultCode=' + IntToStr(ResultCode) + ').')
    else
      Log('CleanupOriginalUserStaleHkcuOle returned False (ResultCode=' + IntToStr(ResultCode) + ').');
  except
    { Non-fatal: e.g. silent install running as SYSTEM or started from an already-elevated shell }
    Log('CleanupOriginalUserStaleHkcuOle raised exception: ' + GetExceptionMessage());
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  InstalledExe, CleanOleExe, CleanProjExe: string;
begin
  if CurStep = ssPostInstall then
  begin
    if IsAdminInstallMode then
    begin
      InstalledExe := ExpandConstant('{app}\{#MyAppExeName}');
      if WizardIsTaskSelected('registerole') then
        CleanOleExe := InstalledExe
      else
        CleanOleExe := '';
      if WizardIsTaskSelected('assocsciprj') then
        CleanProjExe := InstalledExe
      else
        CleanProjExe := '';
      CleanupStaleHkcuOleRegistration(CleanOleExe, CleanProjExe);
      CleanupOriginalUserStaleHkcuOle(CleanOleExe, CleanProjExe);
    end;
    if WizardIsTaskSelected('addtopath') then
      AddDirToPath(ExpandConstant('{app}'));
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usPostUninstall then
  begin
    RemoveDirFromPath(ExpandConstant('{app}'));
    CleanupStaleHkcuOleRegistration('', '');
  end;
end;
