# Generate 12-frame 40x40 pseudo-3D DC motor (SSD1306 page format: 5 pages x 40 cols)
# 3/4 view cylinder body + elliptical end cap + rotating shaft marker on the cap
# 3D cues: ellipse perspective, dithered curvature shading on body top/bottom
# Output: ..\App\anim_frames.h
$SIZE = 40; $NF = 12
$A = 6.5                  # end-cap ellipse semi-axis x (perspective depth)
$B = 11.0                 # end-cap ellipse semi-axis y (= body radius)
$FX = 27.0; $FY = 20.0    # end-cap center
$LX = 8.0                 # left cap center x
$TWO_PI = 2 * [Math]::PI

function AngDiff([double]$a, [double]$b) {
    $d = ($a - $b) % ([Math]::PI * 2)
    if ($d -gt [Math]::PI) { $d -= 2 * [Math]::PI }
    if ($d -lt -[Math]::PI) { $d += 2 * [Math]::PI }
    return [Math]::Abs($d)
}

$allFrames = @()
$previews = @()

for ($f = 0; $f -lt $NF; $f++) {
    $phi = $f / [double]$NF * $TWO_PI
    $pix = New-Object 'bool[,]' $SIZE, $SIZE
    for ($y = 0; $y -lt $SIZE; $y++) {
        for ($x = 0; $x -lt $SIZE; $x++) {
            $on = $false
            $u = ($x - $FX) / $A; $v = ($y - $FY) / $B
            $eFace = $u * $u + $v * $v

            if ($eFace -le 1.0) {
                # ---- end cap (occludes body) ----
                if ($eFace -gt 0.74) { $on = $true }                  # cap rim
                else {
                    $r = [Math]::Sqrt($eFace)
                    $ang = [Math]::Atan2($v, $u)
                    if ($r -le 0.18) { $on = $true }                  # shaft hub
                    elseif ($r -gt 0.24 -and $r -lt 0.74) {
                        if ((AngDiff $ang $phi) -lt 0.40) { $on = $true }          # main spoke
                        elseif ((AngDiff $ang ($phi + [Math]::PI)) -lt 0.26 -and $r -lt 0.5) {
                            $on = $true }                                          # short counter-spoke
                    }
                }
            }
            elseif ($x -le $FX) {
                # ---- cylinder body ----
                $inBody = $false
                if ([Math]::Abs($y - $FY) -le $B) {
                    if ($x -ge $LX) { $inBody = $true }
                    else {
                        $u2 = ($x - $LX) / $A; $v2 = ($y - $FY) / $B
                        if (($u2 * $u2 + $v2 * $v2) -le 1.0) { $inBody = $true }   # left cap round
                    }
                }
                if ($inBody) {
                    $nv = [Math]::Abs($y - $FY) / $B
                    if ($nv -gt 0.86) { $on = $true }                 # top/bottom edge
                    elseif ($x -lt $LX) {
                        $u2 = ($x - $LX) / $A; $v2 = ($y - $FY) / $B
                        $e2 = $u2 * $u2 + $v2 * $v2
                        if ($e2 -gt 0.80) { $on = $true }             # left cap arc
                        elseif ($nv -gt 0.55 -and ((($x + $y) % 2) -eq 0)) { $on = $true }
                    }
                    elseif ($nv -gt 0.55) {
                        if ((($x + $y) % 2) -eq 0) { $on = $true }    # curvature dither
                    }
                }
            }

            if (-not $on) {
                # ---- output shaft (right of cap) ----
                $sx0 = $FX + $A
                if ($x -gt $sx0 -and $x -le ($sx0 + 6) -and [Math]::Abs($y - $FY) -le 1.5) { $on = $true }
                # ---- terminal nubs (left of left cap) ----
                $tx = $LX - $A
                if ($x -ge ($tx - 2) -and $x -lt $tx) {
                    if ([Math]::Abs($y - ($FY - 5)) -le 1 -or [Math]::Abs($y - ($FY + 5)) -le 1) { $on = $true }
                }
            }

            if ($on) { $pix[$x, $y] = $true }
        }
    }
    # page format: page p, col x -> byte bit(row) = pixel(x, p*8+row)
    # NOTE: PowerShell variable names are case-insensitive, so the loop variable
    # must not be named $b - it would clobber the global ellipse axis $B.
    $bytes = @()
    for ($p = 0; $p -lt ($SIZE / 8); $p++) {
        for ($x = 0; $x -lt $SIZE; $x++) {
            $byteVal = 0
            for ($row = 0; $row -lt 8; $row++) {
                if ($pix[$x, ($p * 8 + $row)]) { $byteVal = $byteVal -bor (1 -shl $row) }
            }
            $bytes += $byteVal
        }
    }
    $allFrames += , $bytes

    $pv = "/* ---- frame $f ----`r`n"
    for ($y = 0; $y -lt $SIZE; $y += 2) {
        $line = ""
        for ($x = 0; $x -lt $SIZE; $x++) {
            $top = $pix[$x, $y]; $bot = $pix[$x, ($y + 1)]
            if ($top -and $bot) { $line += '#' }
            elseif ($top) { $line += '^' }
            elseif ($bot) { $line += ',' }
            else { $line += ' ' }
        }
        $pv += $line + "`r`n"
    }
    $pv += "*/"
    $previews += $pv
}

$out = New-Object System.Text.StringBuilder
[void]$out.AppendLine("/**")
[void]$out.AppendLine("  * @file    anim_frames.h")
[void]$out.AppendLine("  * @brief   40x40 pseudo-3D DC motor, 12 frames cover 360deg (shaft marker on end cap)")
[void]$out.AppendLine("  *          SSD1306 page format: 5 pages x 40 cols = 200 bytes/frame, 2400 total")
[void]$out.AppendLine("  *          Generated by Tools/gen_anim.ps1 - do not edit by hand")
[void]$out.AppendLine("  */")
[void]$out.AppendLine("#ifndef __ANIM_FRAMES_H")
[void]$out.AppendLine("#define __ANIM_FRAMES_H")
[void]$out.AppendLine("")
[void]$out.AppendLine("#include <stdint.h>")
[void]$out.AppendLine("")
[void]$out.AppendLine("#define ANIM_FRAME_COUNT   $NF")
[void]$out.AppendLine("#define ANIM_SIZE_PX       $SIZE")
[void]$out.AppendLine("#define ANIM_PAGES         $($SIZE/8)")
[void]$out.AppendLine("")
[void]$out.AppendLine("static const uint8_t ANIM_FRAMES[ANIM_FRAME_COUNT][ANIM_PAGES * ANIM_SIZE_PX] =")
[void]$out.AppendLine("{")
for ($f = 0; $f -lt $NF; $f++) {
    [void]$out.AppendLine($previews[$f])
    [void]$out.AppendLine("    {")
    $bytes = $allFrames[$f]
    for ($i = 0; $i -lt $bytes.Count; $i += 20) {
        $chunk = ($bytes[$i..([Math]::Min($i + 19, $bytes.Count - 1))] | ForEach-Object { "0x{0:X2}" -f $_ }) -join ", "
        [void]$out.AppendLine("        $chunk,")
    }
    [void]$out.AppendLine("    },")
}
[void]$out.AppendLine("};")
[void]$out.AppendLine("")
[void]$out.AppendLine("#endif /* __ANIM_FRAMES_H */")

$dest = Join-Path (Split-Path $PSScriptRoot -Parent) "App\anim_frames.h"
[System.IO.File]::WriteAllText($dest, $out.ToString(), (New-Object System.Text.UTF8Encoding $false))
Write-Host "OK: $NF frames, $($allFrames[0].Count) bytes/frame"
