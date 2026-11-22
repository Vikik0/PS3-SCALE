[CmdletBinding()]
param(
    [string]$Python = "python",
    [string]$Compiler = ""
)

$ErrorActionPreference = "Stop"
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$BuildRoot = Join-Path $env:TEMP "ghosts-coop-host-tests"
New-Item -ItemType Directory -Force -Path $BuildRoot | Out-Null
Set-Location $ProjectRoot

if (-not $Compiler) {
    foreach ($candidate in @("gcc", "clang", "cc")) {
        $found = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($found) {
            $Compiler = $found.Source
            break
        }
    }
}
if (-not $Compiler) {
    throw "No host C compiler found. Install GCC (for example MSYS2 MinGW-w64) or LLVM Clang, then rerun this script."
}
if (-not (Get-Command $Python -ErrorAction SilentlyContinue)) {
    throw "Python command '$Python' was not found. Pass -Python with the correct executable name."
}

Write-Host "== Ghosts co-op host test suite =="
Write-Host "Project: $ProjectRoot"
Write-Host "Compiler: $Compiler"
Write-Host "Build: $BuildRoot"

& $Python -m compileall -q server tests tools
if ($LASTEXITCODE -ne 0) { throw "Python compileall failed." }

& $Python -m unittest discover -s tests -v
if ($LASTEXITCODE -ne 0) { throw "Python unit tests failed." }

function Invoke-CBuildRun {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string[]]$Sources
    )
    $Output = Join-Path $BuildRoot ($Name + ".exe")
    Write-Host ([Environment]::NewLine + "== C test: $Name ==")
    & $Compiler -std=c11 -O2 -Wall -Wextra -Werror @Sources -o $Output
    if ($LASTEXITCODE -ne 0) { throw "Compilation failed for $Name." }
    & $Output
    if ($LASTEXITCODE -ne 0) { throw "Test executable failed: $Name." }
}

Write-Host ([Environment]::NewLine + "== C test: protocol-header-smoke ==")
& $Compiler -std=c11 -Wall -Wextra -Werror -fsyntax-only tests/protocol_header_smoke.c
if ($LASTEXITCODE -ne 0) { throw "Protocol header smoke test failed." }

Invoke-CBuildRun "ghc-protocol" @("client/ps3/ghc_protocol.c", "tests/protocol_c_test.c")
Invoke-CBuildRun "ghc-websocket" @("client/ps3/ghc_websocket.c", "tests/websocket_frame_test.c")
Invoke-CBuildRun "mission-rules" @("client/ps3/mission_rules.c", "tests/mission_rules_test.c")
Invoke-CBuildRun "local-input" @("client/ps3/local_input.c", "tests/local_input_test.c")
Invoke-CBuildRun "local-player-gate" @("client/ps3/local_player_gate.c", "tests/local_player_gate_test.c")
Invoke-CBuildRun "ghc-lobby-json" @("client/ps3/ghc_lobby_json.c", "tests/lobby_json_test.c")
Invoke-CBuildRun "ghc-stun" @("client/ps3/ghc_stun.c", "tests/stun_test.c")
Invoke-CBuildRun "ghc-game-protocol" @("client/ps3/ghc_game_protocol.c", "tests/game_protocol_test.c")
Invoke-CBuildRun "ghosts-runtime-sim" @("client/ps3/local_input.c", "client/ps3/local_player_gate.c", "client/ps3/mission_rules.c", "tests/ghosts_runtime_sim_test.c")

Write-Host ([Environment]::NewLine + "ALL HOST TESTS PASSED")
Write-Host "These are portable/modelled tests, not proof of an in-game Campaign player-two implementation."
