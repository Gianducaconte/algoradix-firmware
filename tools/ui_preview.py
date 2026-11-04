#!/usr/bin/env python3
"""ui_preview.py <dir with .pgm> — one enlarged contact sheet (sheet.png) of the 64x32 screens, with the
faceplate's pill-shaped window drawn over each (about 6 px hidden at both ends)."""
import sys, os, glob
from PIL import Image, ImageDraw
d = sys.argv[1]; files = sorted(glob.glob(os.path.join(d, '*.pgm'))); S = 6; cols = 4
rows = (len(files) + cols - 1) // cols; cw, ch = 64 * S + 24, 32 * S + 40
sheet = Image.new('RGB', (cols * cw + 24, rows * ch + 24), (245, 245, 245)); dr = ImageDraw.Draw(sheet)
for i, f in enumerate(files):
    im = Image.open(f).convert('L').resize((64 * S, 32 * S), Image.NEAREST).convert('RGB')
    x, y = 24 + (i % cols) * cw, 24 + (i // cols) * ch
    mask = Image.new('L', im.size, 0); ImageDraw.Draw(mask).rounded_rectangle([5 * S, 0, 59 * S - 1, 32 * S - 1], radius=15 * S, fill=255)
    dim = Image.blend(im, Image.new('RGB', im.size, (120, 30, 30)), 0.6)                       # hidden part tinted
    sheet.paste(Image.composite(im, dim, mask), (x, y))
    dr.text((x, y + 32 * S + 4), os.path.basename(f)[:-4], fill=(90, 90, 90))
sheet.save(os.path.join(d, 'sheet.png')); print('->', os.path.join(d, 'sheet.png'))
