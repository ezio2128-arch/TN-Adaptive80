[CmdletBinding()]
param([int]$Seconds=60, [string]$Output='FramePulse-overhead.csv')
$ErrorActionPreference='Stop'
if ($Seconds -lt 5 -or $Seconds -gt 600) { throw 'Seconds must be 5..600.' }
$rows=New-Object System.Collections.Generic.List[object]
$previous=@{}; $cores=[Environment]::ProcessorCount
for ($i=0;$i -lt $Seconds;$i++) {
    $time=Get-Date
    foreach ($p in @(Get-Process FramePulseCore,FramePulseWidget,PresentMon -ErrorAction SilentlyContinue)) {
        $cpu=$p.TotalProcessorTime.TotalSeconds; $percent=$null
        if ($previous.ContainsKey($p.Id)) {
            $dt=($time-$previous[$p.Id].time).TotalSeconds
            if($dt -gt 0) { $percent=100*($cpu-$previous[$p.Id].cpu)/$dt/$cores }
        }
        $previous[$p.Id]=@{time=$time;cpu=$cpu}
        $rows.Add([pscustomobject]@{Timestamp=$time.ToString('o');PID=$p.Id;Process=$p.ProcessName;CPUPercent=$percent;WorkingSetMB=$p.WorkingSet64/1MB;PrivateMB=$p.PrivateMemorySize64/1MB})
    }
    Start-Sleep -Seconds 1
}
$rows | Export-Csv -LiteralPath $Output -NoTypeInformation
Write-Host "Saved process overhead to $Output. GPU and in-game hook costs require the A/B captures described in Docs/BENCHMARKS.md."
