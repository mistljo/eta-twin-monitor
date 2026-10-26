param(
    [string]$Ip = "eta-monitor.local",
    [int[]]$Levels = @(1,2,3,4),
    [int]$Repeats = 2,
    [int]$WaitSec = 30
)

$all = @{}

foreach ($lvl in $Levels) {
    for ($r = 1; $r -le $Repeats; $r++) {
        Write-Host "Scan Level $lvl (Durchlauf $r) ..."
        try {
            Invoke-WebRequest -Uri "http://$Ip/scan?start=1&level=$lvl" -Method POST -TimeoutSec 10 -UseBasicParsing | Out-Null
        } catch {
            Write-Host "  Start fehlgeschlagen: $_"
            continue
        }
        Start-Sleep -Seconds $WaitSec
        try {
            $txt = (Invoke-WebRequest -Uri "http://$Ip/scan" -TimeoutSec 30 -UseBasicParsing).Content
        } catch {
            Write-Host "  Abholen fehlgeschlagen: $_"
            continue
        }
        $obj = $txt | ConvertFrom-Json
        $new = 0
        foreach ($row in $obj.rows) {
            if ($row.k -eq 0) { continue }
            $key = "$($row.k)/$($row.h)/$($row.l)/$($row.n)"
            if (-not $all.ContainsKey($key)) {
                $all[$key] = $row
                $new++
            }
        }
        Write-Host "  count=$($obj.count) neu=$new gesamt=$($all.Count)"
    }
}

$out = $all.Values | Sort-Object k, h, l
$out | ConvertTo-Json -Depth 5 | Out-File -Encoding utf8 _scan_merged.json
Write-Host "`n$($all.Count) eindeutige Zeilen -> _scan_merged.json"
