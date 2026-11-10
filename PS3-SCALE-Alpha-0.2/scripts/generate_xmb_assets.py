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

def generate(font):
    PKG.mkdir(parents=True,exist_ok=True)

    # Bright XMB wallpaper: pale lavender, soft glow and ascending staircase.
    bg=Canvas(1280,720,(250,248,255),(222,214,241))
    for wave in range(4):
        prev=None
        for x in range(0,1281,16):
            y=68+wave*154+math.sin(x*0.004+wave*0.85)*20
            point=(x,y)
            if prev:
                bg.line(prev[0],prev[1],point[0],point[1],
                        (232-wave*4,222-wave*3,250),2)
            prev=point
    for i in range(58):
        x=(i*197+83)%1280; y=(i*113+29)%660
        bg.rect(x,y,2,2,(225,216,245))
    bg.circle(1030,132,96,(224,211,248),3)
    bg.circle(1030,132,78,(237,229,251),2)
    stairs(bg,34,8,704,23,18,890,19,
           (255,254,255),(226,220,242),(190,178,220))
    bg.line(20,678,720,48,(208,187,244),4)
    bg.line(40,704,760,74,(242,231,255),3)
    bg.rect(720,280,510,226,(250,248,255))
    bg.rect(720,280,510,3,(187,151,246))
    bg.text(760,326,"PS3",6,(35,29,58),font)
    bg.text(940,326,"SCALE",6,(143,65,221),font)
    bg.text(760,430,"PS3 & PS2 ENHANCEMENT",2,(100,87,132),font)
    bg.text(760,468,"MANAGEMENT",3,(100,87,132),font)
    bg.save(PKG/"PIC1.PNG")

    # XMB icon remains bright and legible at compact PS3 menu size.
    icon=Canvas(320,176,(252,250,255),(231,224,246))
    stairs(icon,9,-10,166,8,12,190,15,
           (255,255,255),(230,223,246),(192,179,224))
    icon.circle(52,58,31,(207,188,245),2)
    icon.text(112,38,"PS3",4,(35,29,58),font)
    icon.text(112,80,"SCALE",4,(143,65,221),font)
    icon.rect(112,128,180,2,(180,147,236))
    icon.text(118,140,"GAME ENHANCER",1,(95,83,126),font)
    icon.save(PKG/"ICON0.PNG")

    # Small preview uses the same white/lavender staircase identity.
    pic0=Canvas(310,176,(250,248,255),(224,216,242))
    stairs(pic0,11,-18,174,8,11,250,14,
           (255,255,255),(230,223,246),(193,180,224))
    pic0.text(18,20,"PS3 SCALE",3,(43,34,68),font)
    pic0.text(20,78,"GRAPHICS / GAME PROFILES",1,(126,93,173),font)
    pic0.save(PKG/"PIC0.PNG")

    for name,size in (("ICON0.PNG",(320,176)),("PIC1.PNG",(1280,720)),("PIC0.PNG",(310,176))):
        data=(PKG/name).read_bytes()
        if data[:8]!=b"\x89PNG\r\n\x1a\n": raise RuntimeError(f"{name} is not PNG")
        dims=struct.unpack_from(">II",data,16)
        if dims!=size: raise RuntimeError(f"{name} dimensions {dims}, expected {size}")
        print(f"XMB asset ready: {name} {dims[0]}x{dims[1]}, {len(data):,} bytes")

if __name__=="__main__":
    generate(load_font())
