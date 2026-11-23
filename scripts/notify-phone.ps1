<#
.SYNOPSIS
    Send a push notification to a smartphone via ntfy.sh (JSON UTF-8 API).
.DESCRIPTION
    Resolves the secret ntfy topic from:
      1. -Topic parameter
      2. $env:TORIGRAPH_NTFY_TOPIC or $env:AGY_NTFY_TOPIC
      3. <repo>/.ntfy-topic (gitignored)
      4. $HOME/.ntfy-topic
    Posts JSON to https://ntfy.sh so Japanese titles and messages never garble.
#>
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Message,

    [string]$Title = "ToriGraph",

    [ValidateRange(1, 5)]
    [int]$Priority = 3,

    [string[]]$Tags = @("white_check_mark"),

    [string]$Topic = "",

    [string]$ServerUrl = "https://ntfy.sh"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot

if (-not $Topic) {
    if ($env:TORIGRAPH_NTFY_TOPIC) {
        $Topic = $env:TORIGRAPH_NTFY_TOPIC.Trim()
    } elseif ($env:AGY_NTFY_TOPIC) {
        $Topic = $env:AGY_NTFY_TOPIC.Trim()
    } elseif (Test-Path (Join-Path $repoRoot ".ntfy-topic")) {
        $Topic = (Get-Content -Raw -Path (Join-Path $repoRoot ".ntfy-topic") -Encoding UTF8).Trim()
    } elseif (Test-Path (Join-Path $HOME ".ntfy-topic")) {
        $Topic = (Get-Content -Raw -Path (Join-Path $HOME ".ntfy-topic") -Encoding UTF8).Trim()
    }
}

if (-not $Topic) {
    throw "ntfy topic not configured. Put your topic in '$repoRoot\.ntfy-topic' or set `$env:AGY_NTFY_TOPIC."
}

$payloadObj = @{
    topic    = $Topic
    title    = $Title
    message  = $Message
    priority = $Priority
    tags     = $Tags
}

$jsonBytes = [System.Text.Encoding]::UTF8.GetBytes(($payloadObj | ConvertTo-Json -Compress))
$endpoint = $ServerUrl.TrimEnd('/')

$resp = Invoke-RestMethod -Method Post -Uri $endpoint -Body $jsonBytes -ContentType "application/json; charset=utf-8" -TimeoutSec 15
Write-Host ("[OK] Sent ntfy notification (id={0}, topic={1})" -f $resp.id, $resp.topic)
