<#
.SYNOPSIS
  センサーの方向ごとの偏りを測って、keyboard_mouse.ino の SCALE_* の値を提案します。

.DESCRIPTION
  keyboard_mouse.ino は、USB シリアルポートが開かれている間、50ms ごとに
  "D,<ms>,<右>,<左>,<下>,<上>,<レポート数>" (SCALE_* をかける前のセンサーのカウント数) を出力します。
  このスクリプトはそれを読み取り、方向ごとに集計します。

  指を同じ 2 点の間で往復させれば、実際に動いた距離は行きと帰りで同じなので、
  カウント数の比がそのままセンサーの偏りになります。

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\measure_direction.ps1 -Port COM9

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\measure_direction.ps1 -Port COM9 -OutCsv measure.csv

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File tools\measure_direction.ps1 -InputCsv measure.csv
#>
param(
  # Arduino のシリアルポート (例: COM9)
  [string]$Port,
  # 測定の最長時間 (秒)。Enter キーでも終了できます
  [int]$Seconds = 120,
  # 測定データを CSV に保存するときのファイル名
  [string]$OutCsv,
  # 保存した CSV を集計するときのファイル名 (Arduino は不要)
  [string]$InputCsv
)

if (-not $Port -and -not $InputCsv) {
  Write-Host '使い方: -Port COM9 でセンサーを測定するか、-InputCsv <ファイル> で保存したデータを集計します'
  exit 1
}

function Read-Rows($lines) {
  foreach ($line in $lines) {
    $f = $line.Trim().Split(',')
    if ($f.Count -ge 7 -and $f[0] -eq 'D') {
      [pscustomobject]@{ Ms = [long]$f[1]; Right = [long]$f[2]; Left = [long]$f[3]; Down = [long]$f[4]; Up = [long]$f[5] }
    }
  }
}

function Test-EnterPressed {
  try {
    while ([Console]::KeyAvailable) {
      if ([Console]::ReadKey($true).Key -eq 'Enter') { return $true }
    }
  } catch {
    # Console input is redirected; only the time limit stops the measurement
  }
  return $false
}

# ---------- 測定 ----------
if ($InputCsv) {
  $lines = Get-Content $InputCsv
} else {
  $serial = New-Object System.IO.Ports.SerialPort $Port, 115200
  $serial.DtrEnable = $true  # The sketch prints only while DTR is set
  $serial.ReadTimeout = 200
  try {
    $serial.Open()
  } catch {
    Write-Host "$Port を開けませんでした: $($_.Exception.Message)"
    Write-Host 'ポート名と、Arduino IDE のシリアルモニタなどが同じポートを開いていないかを確認してください。'
    exit 1
  }

  Write-Host "$Port で測定しています。"
  Write-Host '  1. 指を同じ 2 点の間で、左右に 20 往復ほど動かします (最後は最初の位置に戻す)'
  Write-Host '  2. 少し止めてから、上下も同じように 20 往復ほど動かします'
  Write-Host "  終わったら Enter キーを押してください (最長 $Seconds 秒)。"
  Write-Host ''

  $lines = New-Object System.Collections.Generic.List[string]
  $sum = @{ Right = 0; Left = 0; Down = 0; Up = 0 }
  $deadline = (Get-Date).AddSeconds($Seconds)
  $nextStatus = Get-Date
  try {
    while ((Get-Date) -lt $deadline -and -not (Test-EnterPressed)) {
      try {
        $line = $serial.ReadLine().Trim()
      } catch [System.TimeoutException] {
        $line = ''
      }
      foreach ($row in Read-Rows @($line)) {
        $lines.Add($line)
        $sum.Right += $row.Right; $sum.Left += $row.Left; $sum.Down += $row.Down; $sum.Up += $row.Up
      }
      if ((Get-Date) -ge $nextStatus) {
        Write-Host -NoNewline ("`r  右 {0,6}   左 {1,6}   下 {2,6}   上 {3,6}" -f $sum.Right, $sum.Left, $sum.Down, $sum.Up)
        $nextStatus = (Get-Date).AddMilliseconds(500)
      }
    }
  } finally {
    $serial.Close()
  }
  Write-Host ''
  Write-Host ''

  if ($OutCsv) {
    $lines | Set-Content -Encoding ASCII $OutCsv
    Write-Host "測定データを $OutCsv に保存しました。"
  }
}

# ---------- 集計 ----------
$rows = @(Read-Rows $lines)
if ($rows.Count -eq 0) {
  Write-Host 'データがありません。センサーを動かしたか、keyboard_mouse.ino が書き込まれているかを確認してください。'
  exit 1
}

# Each 50 ms window counts toward the axis it mostly moved along, so the small wobble
# across the axis while moving back and forth does not affect the other axis
$horizontal = @($rows | Where-Object { $_.Right + $_.Left -ge $_.Down + $_.Up })
$vertical = @($rows | Where-Object { $_.Right + $_.Left -lt $_.Down + $_.Up })

function Get-Sum($rows, $name) {
  $s = ($rows | Measure-Object -Property $name -Sum).Sum
  if ($s) { [long]$s } else { 0 }
}

# Splits the movement along one axis into strokes (runs in the same direction)
function Get-Strokes($rows, $pos, $neg, $otherA, $otherB) {
  $strokes = New-Object System.Collections.Generic.List[object]
  $current = $null
  $lastMs = 0
  foreach ($r in $rows) {
    $net = $r.$pos - $r.$neg
    $other = $r.$otherA + $r.$otherB
    if ([math]::Abs($net) -lt 2 -or [math]::Abs($net) -lt $other) { continue }
    $dir = [math]::Sign($net)
    if ($current -and $current.Dir -eq $dir -and $r.Ms - $lastMs -le 150) {
      $current.Counts += [math]::Abs($net)
    } else {
      if ($current) { $strokes.Add($current) }
      $current = [pscustomobject]@{ Dir = $dir; Counts = [math]::Abs($net) }
    }
    $lastMs = $r.Ms
  }
  if ($current) { $strokes.Add($current) }
  # Ignore tiny strokes such as a twitch at the turning point
  return @($strokes | Where-Object { $_.Counts -ge 20 })
}

function Get-Median($values) {
  $sorted = @($values | Sort-Object)
  if ($sorted.Count -eq 0) { return 0 }
  $mid = [int][math]::Floor($sorted.Count / 2)
  if ($sorted.Count % 2) { return $sorted[$mid] }
  return ($sorted[$mid - 1] + $sorted[$mid]) / 2
}

function Show-Axis($title, $nameA, $nameB, $totalA, $totalB, $strokesA, $strokesB, $defineA, $defineB) {
  Write-Host "[$title]"
  Write-Host ("  {0}: 合計 {1,6}   往復 {2,3} 回   1 回あたり {3,6:0.0}" -f $nameA, $totalA, $strokesA.Count, (Get-Median ($strokesA | ForEach-Object Counts)))
  Write-Host ("  {0}: 合計 {1,6}   往復 {2,3} 回   1 回あたり {3,6:0.0}" -f $nameB, $totalB, $strokesB.Count, (Get-Median ($strokesB | ForEach-Object Counts)))
  if ($totalA -lt 200 -or $totalB -lt 200) {
    Write-Host '  データが足りません。もう少し大きく、回数を多く動かしてください。'
    Write-Host ''
    return
  }
  $ratio = $totalA / $totalB
  Write-Host ("  → {0}は{1}の {2:0.00} 倍" -f $nameA, $nameB, $ratio)
  $diff = [math]::Abs($strokesA.Count - $strokesB.Count)
  if ($diff -gt [math]::Max(2, 0.2 * [math]::Max($strokesA.Count, $strokesB.Count))) {
    Write-Host ("  注意: 往復の回数が{0}と{1}でずれています。指を同じ 2 点の間で往復させたか確認してください。" -f $nameA, $nameB)
  }
  # Correct both sides by the square root of the ratio, so the average speed stays the same
  Write-Host '  推奨値:'
  Write-Host ("    #define {0} {1:0.00}" -f $defineA, [math]::Sqrt($totalB / $totalA))
  Write-Host ("    #define {0} {1:0.00}" -f $defineB, [math]::Sqrt($totalA / $totalB))
  Write-Host ''
}

$hStrokes = Get-Strokes $horizontal 'Right' 'Left' 'Down' 'Up'
$vStrokes = Get-Strokes $vertical 'Down' 'Up' 'Right' 'Left'

Show-Axis '左右' '左' '右' (Get-Sum $horizontal 'Left') (Get-Sum $horizontal 'Right') `
  @($hStrokes | Where-Object { $_.Dir -lt 0 }) @($hStrokes | Where-Object { $_.Dir -gt 0 }) 'SCALE_LEFT' 'SCALE_RIGHT'
Show-Axis '上下' '上' '下' (Get-Sum $vertical 'Up') (Get-Sum $vertical 'Down') `
  @($vStrokes | Where-Object { $_.Dir -lt 0 }) @($vStrokes | Where-Object { $_.Dir -gt 0 }) 'SCALE_UP' 'SCALE_DOWN'

Write-Host '推奨値は SCALE_* をかける前のカウント数から計算しているので、今の値に関係なくそのまま書き換えて使えます。'
