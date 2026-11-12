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

def spiral_staircase(canvas, count, center_x, center_y, center_z,
                      focal, outer_radius, inner_radius, angle_step, rise):
    """Render a static, perspective spiral staircase for the XMB wallpaper."""
    faces = []

    def world(radius, angle, y):
        return (center_x + math.cos(angle) * radius,
                center_y + y,
                center_z + math.sin(angle) * radius)

    def project(point):
        x, y, z = point
        z = max(4.0, z)
        return (640.0 + x * focal / z,
                360.0 - y * focal / z)

    for i in range(count):
        angle = i * angle_step + 0.48
        next_angle = angle + angle_step * 0.92
        y = (i - (count - 1) * 0.5) * rise
        top = [
            world(inner_radius, angle, y + 0.14),
            world(outer_radius, angle, y + 0.14),
            world(outer_radius, next_angle, y + 0.14),
            world(inner_radius, next_angle, y + 0.14),
        ]
        riser = [
            world(outer_radius, angle, y - 0.20),
            world(outer_radius, next_angle, y - 0.20),
            top[2], top[1],
        ]
        side = [
            world(inner_radius, next_angle, y - 0.20),
            riser[1], top[2], top[3],
        ]
        faces.append((sum(p[2] for p in top) / 4.0,
                      [project(p) for p in top],
                      (195, 178, 239), (247, 241, 255)))
        faces.append((sum(p[2] for p in riser) / 4.0,
                      [project(p) for p in riser],
                      (103, 80, 160), (202, 186, 235)))
        faces.append((sum(p[2] for p in side) / 4.0,
                      [project(p) for p in side],
                      (143, 119, 197), (224, 211, 248)))

    # Far treads are painted first; nearer treads cover them naturally.
    faces.sort(key=lambda item: item[0], reverse=True)
    for depth, points, fill, edge in faces:
        canvas.poly(points, fill)
        for j in range(4):
            a, b = points[j], points[(j + 1) % 4]
            canvas.line(a[0], a[1], b[0], b[1], edge, 2)

    # Central support and a continuous illuminated outer handrail.
    pole_top = project((center_x, 11.0, center_z))
    pole_bottom = project((center_x, -11.0, center_z))
    canvas.line(pole_top[0], pole_top[1], pole_bottom[0], pole_bottom[1],
                (139, 113, 194), 8)
    canvas.line(pole_top[0], pole_top[1], pole_bottom[0], pole_bottom[1],
                (247, 239, 255), 2)

    rail = []
    for j in range(count * 6 + 1):
        t = j / 6.0
        angle = t * angle_step + 0.48
        y = (t - (count - 1) * 0.5) * rise + 1.15
        rail.append(project(world(outer_radius + 0.62, angle, y)))
    for j in range(len(rail) - 1):
        canvas.line(rail[j][0], rail[j][1], rail[j + 1][0],
                    rail[j + 1][1], (247, 239, 255), 4)
        canvas.line(rail[j][0], rail[j][1], rail[j + 1][0],
                    rail[j + 1][1], (151, 119, 220), 1)

    for i in range(0, count, 3):
        angle = i * angle_step + 0.48
        y = (i - (count - 1) * 0.5) * rise
        base = project(world(outer_radius, angle, y + 0.18))
        top = project(world(outer_radius + 0.62, angle, y + 1.15))
        canvas.line(base[0], base[1], top[0], top[1], (199, 180, 242), 2)


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
    spiral_staircase(bg,28,-10.5,0.0,58.0,1050.0,12.0,1.6,0.44,0.66)
    bg.line(4,686,742,38,(203,184,241),3)
    bg.line(26,710,774,64,(247,239,255),2)
    bg.line(0,640,694,6,(232,221,251),2)
    bg.save(PKG/"PIC1.PNG")

    # Dark, legible XMB tile: the staircase stays on the left and the
    # PS3 SCALE wordmark sits on an uncluttered midnight-purple panel.
    icon=Canvas(320,176,(30,25,54),(5,7,22))
    stairs(icon,9,-36,174,8,10,156,10,
           (255,255,255),(224,217,243),(150,137,190))
    icon.rect(112,10,196,156,(10,10,29))
    icon.line(112,10,308,10,(107,83,160),2)
    icon.line(308,10,308,166,(107,83,160),2)
    icon.line(308,166,112,166,(107,83,160),2)
    icon.line(112,166,112,10,(107,83,160),2)
    ps_mark(icon,202,18,0.76,(255,255,255))
    icon.text(132,58,"PS3",4,(255,255,255),font)
    icon.text(132,98,"SCALE",4,(190,145,255),font)
    icon.rect(132,137,154,2,(151,104,231))
    icon.text(136,149,"GAME ENHANCER",1,(228,220,248),font)
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
