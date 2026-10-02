"""Original Antarctic scenery authored directly on the native pixel grid.

Screens are deduplicated into 8x8, 2bpp tiles for CGB VRAM bank 1.
No source images or resampling; only the Python standard library is required.
"""

class Canvas:
    def __init__(self, height):
        self.height = height
        self.pixels = [[0] * 160 for _ in range(height)]

    def dot(self, x, y, color):
        if 0 <= x < 160 and 0 <= y < self.height:
            self.pixels[y][x] = color

    def line(self, points, color):
        for (x0, y0), (x1, y1) in zip(points, points[1:]):
            dx, dy = abs(x1 - x0), -abs(y1 - y0)
            sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
            err = dx + dy
            while True:
                self.dot(x0, y0, color)
                if (x0, y0) == (x1, y1):
                    break
                twice = 2 * err
                if twice >= dy:
                    err += dy
                    x0 += sx
                if twice <= dx:
                    err += dx
                    y0 += sy

    def poly(self, points, color):
        for y in range(max(0, min(p[1] for p in points)),
                       min(self.height, max(p[1] for p in points) + 1)):
            cuts = []
            for (x0, y0), (x1, y1) in zip(points, points[1:] + points[:1]):
                if min(y0, y1) <= y < max(y0, y1):
                    cuts.append(x0 + (y - y0) * (x1 - x0) // (y1 - y0))
            cuts.sort()
            for left, right in zip(cuts[::2], cuts[1::2]):
                for x in range(max(0, left), min(159, right) + 1):
                    self.dot(x, y, color)

    def rect(self, x, y, w, h, color):
        for row in range(y, y + h):
            for col in range(x, x + w):
                self.dot(col, row, color)


def peak(c, x, y, width, bottom, snow=True):
    """Asymmetric rock faces, cornices, and split ice gullies."""
    c.poly([(x-width, bottom), (x-width//2, y+18), (x-4, y+8),
            (x, y), (x+5, y+13), (x+width//2, y+21), (x+width, bottom)], 2)
    c.poly([(x, y+2), (x+3, y+17), (x-3, y+29), (x+8, y+44),
            (x+5, bottom), (x+width, bottom), (x+width//2, y+21), (x+5, y+13)], 3)
    if snow:
        c.poly([(x,y), (x-4,y+10), (x-width//2,y+18), (x-width//3,y+19),
                (x-width//2,y+33), (x-3,y+20), (x-1,y+13), (x+4,y+18)], 1)
        c.line([(x-4,y+24), (x-8,y+34), (x-7,y+39), (x-15,y+52)], 1)
    c.line([(x+7,y+24), (x+14,y+37), (x+12,y+43), (x+20,bottom-3)], 2)
    # Broken strata follow the planes; no regular speckle/checkerboard fill.
    for depth, inset in ((32, 10), (43, 16), (55, 20), (66, 26)):
        if y + depth + 3 >= bottom:
            continue
        left = max(x - width + 4, x - inset)
        c.line([(left,y+depth+2),(left+5,y+depth),(left+8,y+depth)], 3)
        if snow:
            c.line([(left-1,y+depth+1),(left+4,y+depth-1)], 1)
        c.line([(x+10,y+depth),(x+14,y+depth+1)], 2)


def citadel(c, x, y):
    """Stepped alien masonry with off-center lintels and an open entrance."""
    for dx, dy, w, h in ((-22,9,7,25), (-12,3,9,32), (3,0,8,37), (15,13,6,21)):
        c.rect(x+dx, y+dy, w, h, 3)
        c.rect(x+dx, y+dy, 2, h-2, 2)
        c.rect(x+dx-1, y+dy, w+2, 2, 1)
        c.rect(x+dx+3, y+dy+6, 1, 5, 1)
        c.line([(x+dx+2,y+dy+15),(x+dx+4,y+dy+15),(x+dx+4,y+dy+18)],2)
        c.rect(x+dx-1,y+dy+21,w+1,1,2)
    c.poly([(x-15,y+22), (x-3,y+16), (x+16,y+22), (x+16,y+27), (x-15,y+27)], 3)
    c.line([(x-15,y+22), (x-3,y+16), (x+16,y+22)], 1)
    c.rect(x-8,y+27,20,12,2)
    c.rect(x-3,y+26,9,13,3)
    c.line([(x+8,y+28),(x+10,y+30),(x+8,y+32),(x+10,y+34)],1)
    c.line([(x-3,y+38), (x-3,y+27), (x+5,y+27)], 1)
    for i in range(3):
        c.rect(x-13-i*2,y+39+i*2,30+i*4,1,1)


def title():
    c = Canvas(144)
    for x,y in ((8,10),(29,19),(145,9),(121,17),(11,48),(148,42),(39,56),(118,53),(5,68)):
        c.dot(x,y,2)
    # A waning moon separated from the logo by a clear strip of sky.
    for yy in range(-7,8):
        for xx in range(-7,8):
            if xx*xx + yy*yy <= 49 and (xx-3)**2 + (yy+2)**2 > 42:
                c.dot(145+xx,24+yy,1)
    c.poly([(0,68),(23,59),(60,63),(95,56),(128,59),(159,51),
            (159,54),(129,63),(95,60),(60,66),(23,62),(0,71)],2)
    peak(c,19,76,35,122,False)
    peak(c,136,63,37,125)
    peak(c,113,80,26,123)
    peak(c,58,70,41,129)
    peak(c,91,59,31,129)
    c.poly([(42,119),(69,105),(111,107),(136,122),(125,131),(34,131)],2)
    citadel(c,90,73)
    c.poly([(44,143),(66,123),(79,117),(87,117),(73,127),(66,143)],1)
    c.line([(59,141),(68,131),(72,131),(81,121)],2)
    c.poly([(0,112),(13,115),(28,112),(40,121),(49,124),(59,144),(0,144)],3)
    c.line([(0,112),(13,115),(28,112),(40,121),(49,124)],1)
    c.poly([(126,129),(141,116),(150,119),(160,107),(160,144),(114,144)],3)
    c.line([(128,129),(141,116),(150,119),(159,108)],2)
    for pts in ([(3,121),(18,125),(24,122)],[(7,134),(24,133),(34,140)],[(144,128),(153,124),(157,126)]):
        c.line(pts,2)
    return c


def mountain_map():
    c = Canvas(112)
    peak(c,14,26,29,97,False)
    peak(c,144,14,32,107,False)
    peak(c,38,17,29,109)
    peak(c,122,31,35,112)
    peak(c,79,7,49,112)
    c.poly([(79,31),(91,47),(83,67),(99,87),(91,112),(131,112),(110,77),(106,60),(97,44)],3)
    c.poly([(39,71),(62,51),(56,70),(65,78),(43,102),(41,112),(16,112)],1)
    for pts in ([(39,76),(49,70),(43,85)],[(29,95),(44,86),(38,96)],
                [(51,91),(49,99),(37,108)],[(111,80),(117,96),(113,103)]):
        c.line(pts,2)
    citadel(c,86,0)
    trail = [(30,108),(35,100),(44,93),(57,87),(68,78),(83,75),
             (95,72),(112,66),(118,53),(109,47),(95,44)]
    c.line(trail,3)
    c.line([(x+1,y) for x,y in trail],3)
    for (x0,y0),(x1,y1) in zip(trail,trail[1:]):
        length = max(abs(x1-x0),abs(y1-y0))
        for step in range(0,length,4):
            x,y = x0+(x1-x0)*step//length,y0+(y1-y0)*step//length
            c.rect(x,y,2,1,1)
    for x,y in ((35,103),(67,79),(115,55),(91,31)):
        c.line([(x-5,y),(x+6,y),(x+9,y+2)],1)
        c.line([(x-4,y+2),(x+5,y+2)],3)
    c.poly([(3,103),(11,93),(20,104)],3)
    c.line([(3,103),(11,93),(13,102),(20,104)],1)
    c.line([(1,108),(13,110),(21,108)],2)
    for x,y in ((4,73),(148,74),(136,98)):
        c.poly([(x,y+9),(x+2,y),(x+6,y+1),(x+7,y+10)],3)
        c.line([(x+2,y),(x+2,y+6)],1)
    return c


def encode(canvas):
    tiles, lookup, tilemap = [], {}, []
    for y in range(0,canvas.height,8):
        for x in range(0,160,8):
            data=[]
            for row in canvas.pixels[y:y+8]:
                pixels=row[x:x+8]
                data.extend((sum((p&1)<<(7-i) for i,p in enumerate(pixels)),
                             sum(((p>>1)&1)<<(7-i) for i,p in enumerate(pixels))))
            key=tuple(data)
            if key not in lookup:
                lookup[key]=len(tiles)
                tiles.append(data)
            tilemap.append(lookup[key])
    if len(tiles)>256:
        raise ValueError(f'Scene needs {len(tiles)} tiles; bank 1 holds 256')
    return tiles,tilemap


def export():
    header,source=[],[]
    for name,canvas in (('title_scene',title()),('map_scene',mountain_map())):
        tiles,tilemap=encode(canvas)
        header += [f'#define {name.upper()}_COUNT {len(tiles)}',
                   f'extern const uint8_t {name}_tiles[];',f'extern const uint8_t {name}_map[];']
        for suffix,data in (('tiles',sum(tiles,[])),('map',tilemap)):
            source.append(f'const uint8_t {name}_{suffix}[] = {{')
            for i in range(0,len(data),16):
                source.append('    '+','.join(f'0x{v:02X}' for v in data[i:i+16])+',')
            source.append('};')
        print(f'{name}: {len(tiles)} unique tiles in VRAM bank 1')
    return header,source
