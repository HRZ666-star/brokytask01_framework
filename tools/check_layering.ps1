$ErrorActionPreference = 'Stop'

# tools/check_layering.ps1 lives one level below the repository root.
$root = Split-Path -Parent $PSScriptRoot
$layers = @(
    (Join-Path $root 'app'),
    (Join-Path $root 'modules'),
    (Join-Path $root 'bsp/interface')
)

$files = Get-ChildItem -Path $layers -Recurse -File |
    Where-Object { $_.Extension -in '.c', '.cpp', '.h', '.hpp' }

$bad = $files | Select-String -Pattern 'stm32f1xx_hal\.h|stm32f4xx_hal\.h'
if ($bad) {
    $bad | ForEach-Object {
        Write-Host ("LAYERING ERROR: {0}:{1}: {2}" -f $_.Path, $_.LineNumber, $_.Line.Trim())
    }
    exit 1
}

Write-Host 'layering OK: app/modules/bsp-interface contain no HAL headers'
exit 0
