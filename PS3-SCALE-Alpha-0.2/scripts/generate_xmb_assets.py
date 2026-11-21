#!/usr/bin/env python3
"""Generate PS3 SCALE XMB artwork from the approved pearl-white staircase source."""
from __future__ import annotations
import re, struct, zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PKG = ROOT / "pkgfiles"
FONT_PATH = ROOT / "source/ps3/font8x12.h"
PIC1_SOURCE = ROOT / "assets/xmb/pic1_source.jpg"

def load_font():
    src = FONT_PATH.read_text(encoding="utf-8")
    m = re.search(r"ps3scale_font8x12\[\]\s*=\s*\{(.*?)\};", src, re.S)
    if not m:
        raise RuntimeError("PS3 SCALE bitmap font missing")
    data = bytes(int(v, 16) for v in re.findall(r"0x([0-9a-fA-F]{2})", m.group(1)))
    if len(data) < 95 * 12:
        raise RuntimeError("PS3 SCALE bitmap font is truncated")
    return data

class Canvas:
    def __init__(self, w, h, top, bottom):
        self.w, self.h = w, h
        self.p = bytearray(w*h*3)
        for y in range(h):
            t = y/max(1,h-1)
            c = tuple(int(top[k]*(1-t)+bottom[k]*t) for k in range(3))
            row = bytes(c)*w
            self.p[y*w*3:(y+1)*w*3] = row
    def rect(self, x, y, w, h, c):
        x0,y0=max(0,int(x)),max(0,int(y))
        x1,y1=min(self.w,int(x+w)),min(self.h,int(y+h))
        if x1<=x0 or y1<=y0: return
        row=bytes(c)*(x1-x0)
        for yy in range(y0,y1):
            i=(yy*self.w+x0)*3
            self.p[i:i+len(row)]=row
    def line(self,x0,y0,x1,y1,c,width=1):
        x0,y0,x1,y1=map(int,(x0,y0,x1,y1))
        dx,sx=abs(x1-x0),1 if x0<x1 else -1
        dy,sy=-abs(y1-y0),1 if y0<y1 else -1
        err=dx+dy
        while True:
            self.rect(x0-width//2,y0-width//2,width,width,c)
            if x0==x1 and y0==y1: break
            e=2*err
            if e>=dy: err+=dy; x0+=sx
            if e<=dx: err+=dx; y0+=sy
    def poly(self, pts, c):
        ymin=max(0,int(min(y for x,y in pts)))
        ymax=min(self.h-1,int(max(y for x,y in pts)))
        for y in range(ymin,ymax+1):
            xs=[]
            for i,(x1,y1) in enumerate(pts):
                x2,y2=pts[(i+1)%len(pts)]
                if y1!=y2 and min(y1,y2)<=y<max(y1,y2):
                    xs.append(int(x1+(y-y1)*(x2-x1)/(y2-y1)))
            xs.sort()
            for i in range(0,len(xs)-1,2):
                self.rect(xs[i],y,xs[i+1]-xs[i]+1,1,c)
    def circle(self,cx,cy,r,c,width=2):
        import math
        n=max(36,int(r*2.2)); prev=None
        for i in range(n+1):
            a=i*6.28318530718/n
            p=(cx+math.cos(a)*r,cy+math.sin(a)*r)
            if prev: self.line(prev[0],prev[1],p[0],p[1],c,width)
            prev=p
    def text(self,x,y,value,scale,c,font):
        cur=int(x)
        for ch in value:
            code=ord(ch)
            if code<32 or code>126: code=63
            off=(code-32)*12
            for row in range(12):
                bits=font[off+row]
                for col in range(8):
                    if bits & (0x80>>col):
                        self.rect(cur+col*scale,y+row*scale,scale,scale,c)
            cur+=9*scale
    def save(self,path):
        raw=b"".join(b"\x00"+self.p[y*self.w*3:(y+1)*self.w*3] for y in range(self.h))
        def chunk(k,d):
            return struct.pack(">I",len(d))+k+d+struct.pack(">I",zlib.crc32(k+d)&0xffffffff)
        png=b"\x89PNG\r\n\x1a\n"+chunk(b"IHDR",struct.pack(">IIBBBBB",self.w,self.h,8,2,0,0,0))+chunk(b"IDAT",zlib.compress(raw,9))+chunk(b"IEND",b"")
        Path(path).write_bytes(png)

def stairs(c,n,x0,y0,dx,dy,w0,dw,top,front,side):
    for i in range(n):
        x=x0+i*dx; y=y0-i*dy; w=w0-i*dw
        c.poly([(x+8,y+11),(x+w+18,y-5),(x+w+18,y+25),(x+8,y+42)],side)
        c.poly([(x+8,y+11),(x+w+18,y-5),(x+w+18,y+20),(x+8,y+36)],front)
        c.poly([(x,y),(x+w,y-16),(x+w+18,y-5),(x+8,y+11)],top)
        c.line(x,y,x+w,y-16,(255,252,255),2)
        c.line(x+8,y+11,x+w+18,y-5,(170,145,225),1)

def ps_mark(canvas, x, y, scale, color):
    points = [
        ((0, 0), (0, 28)), ((0, 0), (16, 5)), ((16, 5), (16, 23)),
        ((16, 23), (0, 18)), ((0, 18), (23, 30)), ((23, 30), (34, 25)),
        ((34, 25), (34, 21)), ((34, 21), (23, 17)), ((23, 17), (13, 21))
    ]
    for a, b in points:
        canvas.line(x+a[0]*scale, y+a[1]*scale,
                    x+b[0]*scale, y+b[1]*scale, color, max(1, scale))


def load_display_font(size, bold=False):
    from PIL import ImageFont
    candidates = (
        Path("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf") if bold else
        Path("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"),
        Path("/usr/share/fonts/truetype/liberation2/LiberationSans-Bold.ttf") if bold else
        Path("/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf"),
    )
    for candidate in candidates:
        if candidate.is_file():
            try:
                return ImageFont.truetype(str(candidate), size=size)
            except OSError:
                pass
    return ImageFont.load_default()


def generate(font):
    from PIL import Image, ImageDraw, ImageOps
    PKG.mkdir(parents=True, exist_ok=True)
    if not PIC1_SOURCE.is_file():
        raise RuntimeError(f"Approved XMB staircase source is missing: {PIC1_SOURCE}")

    # PIC1 is the clean 16:9 pearl-white / lavender XMB wallpaper. The
    # staircase stays on the left; the right side is deliberately quiet.
    with Image.open(PIC1_SOURCE) as source_file:
        source = source_file.convert("RGB")
    wallpaper = ImageOps.fit(
        source, (1280, 720), method=Image.LANCZOS
    )
    wallpaper.save(PKG / "PIC1.PNG", format="PNG", optimize=True)

    # ICON0 remains the separate dark-violet package icon so it is legible
    # in the PS3 XMB's small icon slot.
    icon=Canvas(320,176,(30,27,54),(9,10,27))
    for y in range(176):
        t=y/175.0
        icon.rect(0,y,320,1,
                  (int(38-29*t),int(30-20*t),int(64-36*t)))
    stairs(icon,10,-40,177,9,11,163,10,
           (252,251,255),(214,206,237),(140,127,188))
    icon.line(0, 154, 114, 25, (179,142,242), 2)
    icon.line(2, 161, 122, 25, (245,235,255), 1)
    icon.rect(126,8,184,160,(17,15,40))
    icon.line(125,7,311,7,(123,86,205),2)
    icon.line(311,7,311,166,(105,78,173),2)
    icon.line(311,166,125,166,(72,62,125),2)
    icon.line(125,166,125,7,(103,77,159),2)
    icon.line(130,10,305,10,(210,191,249),1)
    ps_mark(icon,234,18,1,(248,247,255))
    icon.text(173,61,"PS3",3,(253,253,255),font)
    icon.text(151,100,"SCALE",3,(197,155,255),font)
    icon.rect(153,141,148,2,(155,94,246))
    icon.rect(153,144,148,1,(83,57,141))
    icon.text(161,151,"GAME ENHANCER",1,(230,218,249),font)
    icon.save(PKG/"ICON0.PNG")

    # PIC0 is the XMB preview tile: the same spiral-staircase scene with a
    # restrained PS3 SCALE identity panel on the right.
    pic0 = ImageOps.fit(
        source, (310, 176), method=Image.LANCZOS
    ).convert("RGBA")
    overlay = Image.new("RGBA", pic0.size, (0, 0, 0, 0))
    draw = ImageDraw.Draw(overlay)
    draw.rounded_rectangle(
        (143, 13, 299, 163), radius=7,
        fill=(251, 249, 255, 232),
        outline=(176, 150, 224, 255), width=2
    )
    draw.line((153, 19, 289, 19), fill=(204, 185, 239, 255), width=1)
    draw.line((158, 139, 285, 139), fill=(177, 145, 226, 255), width=2)
    # A small, clean PlayStation-inspired mark above the wordmark.
    draw.line((221, 28, 221, 47), fill=(48, 39, 76, 255), width=3)
    draw.line((221, 28, 234, 32), fill=(48, 39, 76, 255), width=3)
    draw.line((234, 32, 234, 46), fill=(48, 39, 76, 255), width=3)
    draw.line((221, 46, 237, 52), fill=(48, 39, 76, 255), width=2)

    font_ps3 = load_display_font(25, bold=True)
    font_scale = load_display_font(22, bold=True)
    font_small = load_display_font(8, bold=True)

    def centered_text(label, y, selected_font, color):
        box = draw.textbbox((0, 0), label, font=selected_font)
        width = box[2] - box[0]
        x = 221 - width // 2
        draw.text((x, y), label, font=selected_font, fill=color)

    centered_text("PS3", 62, font_ps3, (43, 35, 69, 255))
    centered_text("SCALE", 94, font_scale, (126, 75, 204, 255))
    centered_text("GAME ENHANCER", 145, font_small, (92, 82, 119, 255))
    pic0 = Image.alpha_composite(pic0, overlay).convert("RGB")
    pic0.save(PKG / "PIC0.PNG", format="PNG", optimize=True)

    for name, expected in (
        ("PIC1.PNG", (1280, 720)),
        ("ICON0.PNG", (320, 176)),
        ("PIC0.PNG", (310, 176)),
    ):
        path = PKG / name
        with Image.open(path) as image_file:
            if image_file.format != "PNG":
                raise RuntimeError(f"{name} is not PNG")
            if image_file.size != expected:
                raise RuntimeError(f"{name} dimensions {image_file.size}, expected {expected}")
        data = path.read_bytes()
        print(f"XMB asset ready: {name} {expected[0]}x{expected[1]}, {len(data):,} bytes")


if __name__=="__main__":
    generate(load_font())
