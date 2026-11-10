#!/usr/bin/env python3
"""Generate PS3 SCALE XMB artwork using only the Python standard library."""
from __future__ import annotations
import re, struct, zlib
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
    bg=Canvas(1280,720,(21,18,46),(94,69,132))
    for i in range(105):
        x=(i*197+83)%1280; y=(i*113+29)%450
        bg.rect(x,y,3 if i%4==0 else 2,3 if i%4==0 else 2,(192,180,230))
    bg.circle(1000,145,70,(214,198,255),3)
    bg.circle(1000,145,58,(154,126,216),2)
    bg.line(880,148,1120,100,(201,183,245),2)
    stairs(bg,23,12,660,22,25,600,15,(247,244,255),(190,180,215),(105,88,145))
    bg.line(40,646,610,76,(193,165,255),5)
    bg.rect(620,285,625,300,(40,31,72))
    bg.rect(620,285,625,3,(184,151,255))
    bg.text(670,330,"PS3",8,(250,247,255),font)
    bg.text(910,330,"SCALE",8,(190,145,255),font)
    bg.rect(670,440,530,5,(176,140,255))
    bg.text(670,475,"PS3 & PS2 ENHANCEMENT MANAGEMENT",2,(239,232,255),font)
    bg.text(670,525,"A NEW WAY TO EXPLORE YOUR GAME LIBRARY",2,(211,199,239),font)
    bg.rect(670,595,430,42,(76,58,112))
    bg.text(688,608,"CFW 4.92  |  PS3 SLIM READY",2,(248,243,255),font)
    bg.save(PKG/"PIC1.PNG")

    icon=Canvas(320,176,(24,18,52),(72,45,117))
    stairs(icon,8,8,142,8,13,124,8,(239,232,255),(169,147,216),(94,70,146))
    icon.line(15,137,88,26,(202,171,255),2)
    icon.text(142,40,"PS3",3,(255,255,255),font)
    icon.text(142,79,"SCALE",3,(193,148,255),font)
    icon.rect(142,128,164,22,(75,54,118))
    icon.text(151,133,"GAME ENHANCER",1,(255,255,255),font)
    icon.save(PKG/"ICON0.PNG")

    pic0=Canvas(310,176,(26,20,56),(77,50,122))
    stairs(pic0,9,4,160,7,13,300,18,(242,236,255),(170,151,213),(95,73,145))
    pic0.text(18,18,"PS3 SCALE",3,(255,255,255),font)
    pic0.text(20,64,"GRAPHICS / GAME PROFILES",1,(205,180,255),font)
    pic0.save(PKG/"PIC0.PNG")

    for name,size in (("ICON0.PNG",(320,176)),("PIC1.PNG",(1280,720)),("PIC0.PNG",(310,176))):
        data=(PKG/name).read_bytes()
        if data[:8]!=b"\x89PNG\r\n\x1a\n": raise RuntimeError(f"{name} is not PNG")
        dims=struct.unpack_from(">II",data,16)
        if dims!=size: raise RuntimeError(f"{name} dimensions {dims}, expected {size}")
        print(f"XMB asset ready: {name} {dims[0]}x{dims[1]}, {len(data):,} bytes")

if __name__=="__main__":
    generate(load_font())
