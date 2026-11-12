#!/usr/bin/env python3
"""Generate PS3 SCALE XMB artwork using only the Python standard library."""
from __future__ import annotations
import math, re, struct, zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PKG = ROOT / "pkgfiles"
FONT_PATH = ROOT / "source/ps3/font8x12.h"

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


def generate(font):
    PKG.mkdir(parents=True,exist_ok=True)

    # XMB wallpaper: clean lavender light, a broad stair sweep on the left,
    # and quiet negative space on the right for the PS3 XMB icons.
    bg=Canvas(1280,720,(253,252,255),(224,217,245))
    for radius, color, width in (
        (260,(236,228,251),3),(220,(242,236,253),2),(178,(247,243,255),2)
    ):
        bg.circle(1050,150,radius,color,width)
    for wave in range(5):
        prev=None
        for x in range(0,1281,12):
            y=90+wave*118+math.sin(x*0.0037+wave*0.78)*22
            point=(x,y)
            if prev:
                bg.line(prev[0],prev[1],point[0],point[1],
                        (238-wave*3,231-wave*3,251),2)
            prev=point
    for i in range(78):
        x=(i*197+83)%1280; y=(i*113+29)%690
        bg.rect(x,y,2 if i%4 else 3,2 if i%4 else 3,(229,220,247))
    stairs(bg,34,-86,710,27,18,1050,21,
           (255,255,255),(230,224,244),(185,173,218))
    bg.line(4,686,742,38,(203,184,241),4)
    bg.line(26,710,774,64,(247,239,255),3)
    bg.line(0,640,694,6,(232,221,251),2)
    bg.save(PKG/"PIC1.PNG")

    # The compact XMB icon carries the actual PS3 SCALE mark and staircase.
    icon=Canvas(320,176,(253,251,255),(228,220,247))
    icon.rect(12,12,296,152,(250,248,255))
    icon.rect(12,12,5,152,(148,91,232))
    icon.circle(64,84,54,(226,214,250),3)
    stairs(icon,10,-12,166,8,10,126,7,
           (255,255,255),(229,223,245),(190,178,223))
    ps_mark(icon,37,38,1,(43,35,68))
    icon.text(126,42,"PS3",4,(43,35,68),font)
    icon.text(126,82,"SCALE",4,(143,65,221),font)
    icon.rect(126,119,164,2,(180,147,236))
    icon.text(130,132,"PS3 & PS2 ENHANCER",1,(100,87,132),font)
    icon.save(PKG/"ICON0.PNG")

    # XMB preview tile matches the installed app icon.
    pic0=Canvas(310,176,(250,248,255),(224,216,242))
    stairs(pic0,12,-24,172,8,10,244,13,
           (255,255,255),(230,223,246),(193,180,224))
    ps_mark(pic0,22,24,1,(43,35,68))
    pic0.text(70,24,"PS3 SCALE",3,(43,35,68),font)
    pic0.text(72,66,"GAME ENHANCEMENT",1,(126,93,173),font)
    pic0.text(72,82,"MANAGEMENT",1,(126,93,173),font)
    pic0.save(PKG/"PIC0.PNG")

    for name,size in (("ICON0.PNG",(320,176)),("PIC1.PNG",(1280,720)),("PIC0.PNG",(310,176))):
        data=(PKG/name).read_bytes()
        if data[:8]!=b"\x89PNG\r\n\x1a\n": raise RuntimeError(f"{name} is not PNG")
        dims=struct.unpack_from(">II",data,16)
        if dims!=size: raise RuntimeError(f"{name} dimensions {dims}, expected {size}")
        print(f"XMB asset ready: {name} {dims[0]}x{dims[1]}, {len(data):,} bytes")

if __name__=="__main__":
    generate(load_font())
