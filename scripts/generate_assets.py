#!/usr/bin/env python3
"""Generate the code-drawn launcher mark (no external image dependencies)."""
from pathlib import Path
from PIL import Image, ImageDraw
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'firmware/codex_monitor/view'
svg='''<svg xmlns="http://www.w3.org/2000/svg" width="70" height="70" viewBox="0 0 70 70"><g fill="none" stroke="black" stroke-width="6" stroke-linejoin="miter"><path d="M55 15 A28 28 0 1 0 55 55"/><path d="M27 25 L37 35 L27 45 M40 45 H51"/></g></svg>'''
(OUT/'codex_icon.svg').write_text(svg+'\n')
scale=4
im=Image.new('L',(70*scale,70*scale),0);d=ImageDraw.Draw(im)
d.arc((7*scale,7*scale,63*scale,63*scale),45,315,fill=255,width=6*scale)
d.line([(27*scale,25*scale),(37*scale,35*scale),(27*scale,45*scale)],fill=255,width=6*scale,joint='curve')
d.line([(40*scale,45*scale),(51*scale,45*scale)],fill=255,width=6*scale)
im=im.resize((70,70),Image.Resampling.LANCZOS)
# Binary alpha gives a crisp mark on the 1-bit display; transparency permits inversion.
im=im.point(lambda a:255 if a>=128 else 0)
data=im.tobytes()
rows=[', '.join(f'0x{x:02x}' for x in data[i:i+20]) for i in range(0,len(data),20)]
(OUT/'codex_icon.c').write_text('#include "lvgl.h"\nstatic const uint8_t pixels[] = {\n'+',\n'.join(rows)+'''\n};
const lv_image_dsc_t codex_app_icon = {
 .header = {.magic=LV_IMAGE_HEADER_MAGIC, .cf=LV_COLOR_FORMAT_A8, .w=70, .h=70, .stride=70},
 .data_size=sizeof(pixels), .data=pixels,
};
''')
rgba=Image.new('RGBA',im.size,(0,0,0,0));rgba.putalpha(im)
rgba.save(ROOT/'docs/ui/codex-icon.png')
# 1-bit transparent stipple: retains the empty ring on a monochrome panel.
# A plain light-gray arc can vanish when the display quantizes to I1.
stride=40
pixels=bytearray(stride*290)
for y in range(290):
    for x in range(290):
        r2=(x-144.5)**2+(y-144.5)**2
        if 119**2<=r2<=145**2 and x%4==0 and y%4<2:
            pixels[y*stride+x//8]|=1<<(7-x%8)
palette=bytes((255,255,255,0,0,0,0,255))
values=palette+pixels
rows=[', '.join(f'0x{x:02x}' for x in values[i:i+20]) for i in range(0,len(values),20)]
with (OUT/'codex_icon.c').open('a') as f:
    f.write('\nstatic const uint8_t track[] = {\n'+',\n'.join(rows)+'''\n};
const lv_image_dsc_t codex_ring_track = {
 .header = {.magic=LV_IMAGE_HEADER_MAGIC, .cf=LV_COLOR_FORMAT_I1, .w=290, .h=290, .stride=40},
 .data_size=sizeof(track), .data=track,
};
''')
stride=52
pixels=bytearray(stride*18)
for y in range(18):
    for x in range(416):
        if x%4==0 and y%4<2: pixels[y*stride+x//8]|=1<<(7-x%8)
values=palette+pixels
rows=[', '.join(f'0x{x:02x}' for x in values[i:i+20]) for i in range(0,len(values),20)]
with (OUT/'codex_icon.c').open('a') as f:
    f.write('\nstatic const uint8_t week_track[] = {\n'+',\n'.join(rows)+'''\n};
const lv_image_dsc_t codex_week_track = {
 .header = {.magic=LV_IMAGE_HEADER_MAGIC, .cf=LV_COLOR_FORMAT_I1, .w=416, .h=18, .stride=52},
 .data_size=sizeof(week_track), .data=week_track,
};
''')
