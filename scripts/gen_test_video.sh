#!/usr/bin/env bash
# Generate the HyperVane animated intro (12 s, 1280x720, H.264).
#
# Visual elements:
#   • 4 rotating electric-blue vane blades emanating from the center
#   • Inner + outer ring glow (the HV emblem)
#   • "HYPERVANE" title fades in at t=2 s
#   • "Intelligent Emulation Frontend" subtitle fades in at t=5 s
#   • Full fade-in and fade-to-black
#
# Usage: bash scripts/gen_test_video.sh [output.mp4]
set -euo pipefail

OUT="${1:-/tmp/hypervane_intro.mp4}"
W=1280; H=720; FPS=30; DUR=12
HW=$((W/2)); HH=$((H/2))   # centre pixel coords

# ── Find a bold font via fontconfig (available in the Nix dev shell) ──────────
FONT=$(fc-match "DejaVu Sans:Bold" --format="%{file}" 2>/dev/null \
       || fc-match ":bold"         --format="%{file}" 2>/dev/null || true)
[[ -z "$FONT" ]] && {
    echo "No font found via fc-match."
    echo "Add 'dejavu_fonts' to buildInputs in flake.nix, then re-enter the shell."
    exit 1
}
echo "Font  : $FONT"
echo "Output: $OUT"

# ── Filter chain (linear — uses -vf, no labels needed) ───────────────────────
#
# Step 1  format=rgb24          force RGB so geq r/g/b channels are unambiguous
# Step 2  geq                   generate vane blades + ring glow per-pixel
# Step 3  format=yuv420p        convert before drawtext & encode (fixes 4:4:4)
# Step 4  drawtext ×2           title + subtitle with animated alpha
# Step 5  fade ×2               fade in / fade to black
#
# Inside geq expressions, commas in function calls (clip\, pow\, etc.)
# must be escaped as \, so FFmpeg does not treat them as filter separators.

VF="format=rgb24,\
geq=\
r='clip(\
  20\
  +200*pow(max(0\,sin(atan2(Y-${HH}\,X-${HW})*4+T*1.5))\,3)*exp(-sqrt((X-${HW})*(X-${HW})+(Y-${HH})*(Y-${HH}))/450)\
  +70*exp(-pow((sqrt((X-${HW})*(X-${HW})+(Y-${HH})*(Y-${HH}))-150)/22\,2))\
  +25*exp(-pow((sqrt((X-${HW})*(X-${HW})+(Y-${HH})*(Y-${HH}))-310)/18\,2))\
  +5*sin(6.2832*X/90+T*2)*sin(6.2832*Y/90+T*2.5)\
\,0\,255)':\
g='clip(\
  8\
  +90*pow(max(0\,sin(atan2(Y-${HH}\,X-${HW})*4+T*1.5))\,3)*exp(-sqrt((X-${HW})*(X-${HW})+(Y-${HH})*(Y-${HH}))/450)\
  +35*exp(-pow((sqrt((X-${HW})*(X-${HW})+(Y-${HH})*(Y-${HH}))-150)/22\,2))\
  +12*exp(-pow((sqrt((X-${HW})*(X-${HW})+(Y-${HH})*(Y-${HH}))-310)/18\,2))\
  +2*sin(6.2832*X/90+T*2)*sin(6.2832*Y/90+T*2.5)\
\,0\,255)':\
b='clip(\
  45\
  +240*pow(max(0\,sin(atan2(Y-${HH}\,X-${HW})*4+T*1.5+0.5))\,3)*exp(-sqrt((X-${HW})*(X-${HW})+(Y-${HH})*(Y-${HH}))/400)\
  +90*exp(-pow((sqrt((X-${HW})*(X-${HW})+(Y-${HH})*(Y-${HH}))-150)/22\,2))\
  +40*exp(-pow((sqrt((X-${HW})*(X-${HW})+(Y-${HH})*(Y-${HH}))-310)/18\,2))\
  +15*sin(6.2832*X/90+T*2)*sin(6.2832*Y/90+T*2.5)\
\,0\,255)',\
format=yuv420p,\
drawtext=\
fontfile='${FONT}':\
fontsize=150:\
fontcolor=white:\
bordercolor=0x1144ff:\
borderw=5:\
text='HYPERVANE':\
x=(w-text_w)/2:\
y=(h-text_h)/2-55:\
alpha='if(lt(t\,2)\,0\,if(lt(t\,4)\,(t-2)/2\,if(lt(t\,10)\,1\,max(0\,1-(t-10)/2))))',\
drawtext=\
fontfile='${FONT}':\
fontsize=36:\
fontcolor=0x7799ff:\
text='Intelligent Emulation Frontend':\
x=(w-text_w)/2:\
y=(h/2)+85:\
alpha='if(lt(t\,5)\,0\,if(lt(t\,7)\,(t-5)/2\,if(lt(t\,10)\,1\,max(0\,1-(t-10)/2))))',\
fade=t=in:st=0:d=1,\
fade=t=out:st=11:d=1"

ffmpeg -y \
    -f lavfi -i "color=c=black:s=${W}x${H}:r=${FPS}:d=${DUR}" \
    -vf "$VF" \
    -c:v libx264 -pix_fmt yuv420p \
    -preset fast -crf 18 \
    -movflags +faststart \
    "$OUT"

echo "Done: $OUT"
