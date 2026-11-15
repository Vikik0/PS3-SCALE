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
                      (249, 247, 254), (255, 254, 255)))
        faces.append((sum(p[2] for p in riser) / 4.0,
                      [project(p) for p in riser],
                      (215, 207, 235), (241, 230, 250)))
        faces.append((sum(p[2] for p in side) / 4.0,
                      [project(p) for p in side],
                      (194, 185, 218), (232, 221, 246)))

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
                (194, 182, 225), 8)
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
                    rail[j + 1][1], (175, 145, 224), 1)

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
    PKG.mkdir(parents=True, exist_ok=True)

    # PIC1: pearl-white cinematic XMB wallpaper. The staircase is the
    # foreground subject, while the right side stays quiet for XMB labels.
    bg = Canvas(1280, 720, (252, 252, 255), (231, 228, 243))
    for y in range(720):
        # Very subtle satin/lavender horizontal highlights, no harsh bands.
        t = y / 719.0
        wave = (math.sin(y * 0.014) + 1.0) * 0.5
        c = (int(253 - 15*t + 2*wave),
             int(252 - 18*t + 2*wave),
             int(255 - 7*t))
        bg.rect(0, y, 1280, 1, c)

    # Wide translucent architectural lighting behind the stairs.
    for r, c, width in (
        (315, (237, 233, 249), 5),
        (260, (245, 242, 253), 4),
        (204, (250, 248, 255), 3)
    ):
        bg.circle(1102, 137, r, c, width)

    # Satin light tracks extend across the 16:9 background.
    for wave in range(4):
        last = None
        for x in range(0, 1281, 10):
            y = 80 + wave*145 + math.sin(x * 0.004 + wave*0.72)*17
            if last:
                bg.line(last[0], last[1], x, y,
                        (232 - wave*3, 225 - wave*3, 247), 2)
            last = (x, y)

    for i in range(58):
        x=(i*197+83)%1280
        y=(i*113+29)%690
        bg.rect(x,y,2 if i%5 else 3,2 if i%5 else 3,(232,224,247))

    # Exact asset dimensions and key call preserved for PS3 UI contract tests.
    spiral_staircase(bg,28,-10.5,0.0,58.0,1050.0,12.0,1.6,0.44,0.66)
    bg.line(0, 686, 759, 18, (200, 182, 232), 2)
    bg.line(14, 717, 790, 35, (254, 250, 255), 3)
    bg.save(PKG/"PIC1.PNG")

    # ICON0: dark violet premium package icon, designed to be readable at
    # real PS3 XMB thumbnail size (320 x 176), not only in a close-up.
    icon=Canvas(320,176,(30,27,54),(9,10,27))
    for y in range(176):
        t=y/175.0
        icon.rect(0,y,320,1,
                  (int(38-29*t),int(30-20*t),int(64-36*t)))
    # Left-side pale steps, with silver risers and soft lavender edges.
    stairs(icon,10,-40,177,9,11,163,10,
           (252,251,255),(214,206,237),(140,127,188))
    icon.line(0, 154, 114, 25, (179,142,242), 2)
    icon.line(2, 161, 122, 25, (245,235,255), 1)
    # Right-side legible matte panel and thin polished frame.
    icon.rect(126,8,184,160,(17,15,40))
    icon.line(125,7,311,7,(123,86,205),2)
    icon.line(311,7,311,166,(105,78,173),2)
    icon.line(311,166,125,166,(72,62,125),2)
    icon.line(125,166,125,7,(103,77,159),2)
    icon.line(130,10,305,10,(210,191,249),1)
    # PlayStation-like line illustration drawn by this project's ps_mark.
    ps_mark(icon,234,18,1,(248,247,255))
    # Two-line wordmark avoids tiny illegible glyphs.
    icon.text(173,61,"PS3",3,(253,253,255),font)
    icon.text(151,100,"SCALE",3,(197,155,255),font)
    icon.rect(153,141,148,2,(155,94,246))
    icon.rect(153,144,148,1,(83,57,141))
    icon.text(161,151,"GAME ENHANCER",1,(230,218,249),font)
    icon.save(PKG/"ICON0.PNG")

    # PIC0: a pearl-white preview tile matching the selected app identity.
    pic0=Canvas(310,176,(252,252,255),(229,226,242))
    for y in range(176):
        t=y/175.0
        pic0.rect(0,y,310,1,(int(251-18*t),int(251-21*t),int(255-11*t)))
    stairs(pic0,12,-28,182,8,10,240,12,
           (253,253,255),(226,221,240),(187,176,214))
    pic0.rect(122,15,178,143,(246,243,252))
    pic0.line(123,15,299,15,(175,144,226),2)
    pic0.line(123,158,299,158,(207,191,233),2)
    ps_mark(pic0,191,22,1,(42,34,70))
    pic0.text(148,74,"PS3",3,(45,36,72),font)
    pic0.text(147,111,"SCALE",3,(120,60,207),font)
    pic0.save(PKG/"PIC0.PNG")

    for name,size in (("PIC1.PNG",(1280,720)),("ICON0.PNG",(320,176)),("PIC0.PNG",(310,176))):
        data=(PKG/name).read_bytes()
        if data[:8]!=b"\x89PNG\r\n\x1a\n":
            raise RuntimeError(f"{name} is not PNG")
        dims=struct.unpack_from(">II",data,16)
        if dims!=size:
            raise RuntimeError(f"{name} dimensions {dims}, expected {size}")
        print(f"XMB asset ready: {name} {dims[0]}x{dims[1]}, {len(data):,} bytes")

if __name__=="__main__":
    generate(load_font())
