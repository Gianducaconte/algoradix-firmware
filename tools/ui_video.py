#!/usr/bin/env python3
"""ui_video.py <dir> — turns <dir>/anim.raw + anim.txt (written by sim/ui_anim.c) into frames of the OLED behind the
faceplate's pill-shaped window, with the gesture caption underneath, then encodes <dir>/ui-navigation.mp4 and .gif
with ffmpeg. The pixels are the firmware's own; only the glow and the bezel are drawn here."""
import sys, os, glob, subprocess
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

W, H, S, FPS = 64, 32, 10, 50
d = sys.argv[1]
raw = open(os.path.join(d, 'anim.raw'), 'rb').read()
caps = open(os.path.join(d, 'anim.txt')).read().split('\n')
n = len(raw) // (W * H)
fdir = os.path.join(d, 'frames'); os.makedirs(fdir, exist_ok=True)
for f in glob.glob(os.path.join(fdir, '*.png')): os.remove(f)

PAD, CAP_H = 40, 70
CW, CH = W * S + 2 * PAD, H * S + 2 * PAD + CAP_H
BG, PANEL, BEZEL, INK = (24, 24, 26), (236, 230, 210), (18, 18, 20), (60, 58, 52)
font = None
for p in ('/System/Library/Fonts/Supplemental/Arial Bold.ttf', '/Library/Fonts/Arial Bold.ttf',
          '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf', os.path.join(os.path.dirname(__file__), 'fonts', 'LiberationSans-Bold.ttf')):
    if os.path.exists(p): font = ImageFont.truetype(p, 26); break
font = font or ImageFont.load_default()

win = Image.new('L', (W * S, H * S), 0)                                   # visible window: about 5 px hidden at each end
ImageDraw.Draw(win).rounded_rectangle([5 * S, 0, 59 * S - 1, H * S - 1], radius=15 * S, fill=255)
for i in range(n):
    px = Image.frombytes('L', (W, H), raw[i * W * H:(i + 1) * W * H]).resize((W * S, H * S), Image.NEAREST)
    grid = Image.new('L', px.size, 255); g = ImageDraw.Draw(grid)          # thin gaps between OLED pixels
    for k in range(0, W * S, S): g.line([k, 0, k, H * S], fill=150)
    for k in range(0, H * S, S): g.line([0, k, W * S, k], fill=150)
    lit = ImageChops.multiply(px, grid)
    glow = px.filter(ImageFilter.GaussianBlur(S * 0.9)).point(lambda v: int(v * 0.55))
    screen = Image.composite(Image.new('RGB', px.size, (235, 244, 255)), Image.new('RGB', px.size, (8, 10, 12)), lit)
    screen = ImageChops.add(screen, Image.merge('RGB', (glow.point(lambda v: int(v * 0.8)), glow.point(lambda v: int(v * 0.9)), glow)))
    frame = Image.new('RGB', (CW, CH), PANEL); fd = ImageDraw.Draw(frame)
    fd.rounded_rectangle([PAD + 5 * S - 14, PAD - 14, PAD + 59 * S + 13, PAD + H * S + 13], radius=15 * S + 14, fill=BEZEL)
    frame.paste(screen, (PAD, PAD), win)
    c = caps[i] if i < len(caps) else ''
    if c:
        tw = fd.textlength(c, font=font)
        fd.text(((CW - tw) / 2, PAD + H * S + PAD - 4), c, fill=INK, font=font)
    frame.save(os.path.join(fdir, '%05d.png' % i))

mp4, gif = os.path.join(d, 'ui-navigation.mp4'), os.path.join(d, 'ui-navigation.gif')
src = ['-framerate', str(FPS), '-i', os.path.join(fdir, '%05d.png')]
subprocess.run(['ffmpeg', '-v', 'error', '-y', *src, '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-crf', '18', '-movflags', '+faststart', mp4], check=True)
subprocess.run(['ffmpeg', '-v', 'error', '-y', *src, '-vf',
                'fps=25,scale=480:-1:flags=lanczos,split[a][b];[a]palettegen=max_colors=64[p];[b][p]paletteuse=dither=none',
                '-loop', '0', gif], check=True)
print('->', mp4, gif, '(%d frames, %.1f s)' % (n, n / FPS))
