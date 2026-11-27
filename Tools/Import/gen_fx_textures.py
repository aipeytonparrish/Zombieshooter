"""Generate Zombieshooter's own FX textures as PNGs (pure Python, no third-party art, no numpy).

  python gen_fx_textures.py <out_dir>

Writes:
  T_ZS_BloodSplat.png   2x2 atlas (4 splatters, 512 px each): RGB = blood colour (darker where thicker), A = coverage
  T_ZS_BloodPool.png    a thick pool with a ragged rim, A = coverage
  T_ZS_BulletHole.png   2x2 atlas of chipped bullet holes in concrete (dark core, crushed ring, hairline cracks)
  T_ZS_Puff.png         soft billowy puff (white; tinted per particle: blood mist, dust)
  T_ZS_Drop.png         a single droplet with a hard wet edge (white; tinted per particle)
Then run create_gore_fx.py inside the editor to import them and build the materials.
"""
import math
import os
import random
import struct
import sys
import zlib


def write_png(path, w, h, pixels):
    """pixels: bytearray RGBA, row-major."""
    raw = bytearray()
    stride = w * 4
    for y in range(h):
        raw.append(0)
        raw.extend(pixels[y * stride:(y + 1) * stride])

    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 6)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


class ValueNoise:
    """Tileable-enough value noise: a few octaves of bilinear-interpolated random grids."""

    def __init__(self, rng, size, cells=(6, 14, 32, 64), weights=(0.5, 0.28, 0.14, 0.08)):
        self.size = size
        self.octaves = []
        for c, wgt in zip(cells, weights):
            grid = [rng.random() for _ in range((c + 1) * (c + 1))]
            self.octaves.append((c, wgt, grid))

    def field(self):
        s = self.size
        out = [0.0] * (s * s)
        for c, wgt, grid in self.octaves:
            step = c / float(s)
            row = c + 1
            for y in range(s):
                fy = y * step
                iy = int(fy)
                ty = fy - iy
                ty = ty * ty * (3 - 2 * ty)
                base0 = iy * row
                base1 = base0 + row
                o = y * s
                for x in range(s):
                    fx = x * step
                    ix = int(fx)
                    tx = fx - ix
                    tx = tx * tx * (3 - 2 * tx)
                    a = grid[base0 + ix]
                    b = grid[base0 + ix + 1]
                    cc = grid[base1 + ix]
                    d = grid[base1 + ix + 1]
                    top = a + (b - a) * tx
                    bot = cc + (d - cc) * tx
                    out[o + x] += (top + (bot - top) * ty) * wgt
        return out


def splat_disc(field, s, cx, cy, r, strength=1.0):
    """Add a smooth metaball (1 - d^2/r^2)^2 into field, bbox only."""
    x0 = max(0, int(cx - r))
    x1 = min(s - 1, int(cx + r) + 1)
    y0 = max(0, int(cy - r))
    y1 = min(s - 1, int(cy + r) + 1)
    inv = 1.0 / (r * r)
    for y in range(y0, y1 + 1):
        dy = (y - cy) * (y - cy)
        o = y * s
        for x in range(x0, x1 + 1):
            q = ((x - cx) * (x - cx) + dy) * inv
            if q < 1.0:
                k = 1.0 - q
                field[o + x] += k * k * strength


def smoothstep(e0, e1, v):
    t = (v - e0) / (e1 - e0)
    if t <= 0.0:
        return 0.0
    if t >= 1.0:
        return 1.0
    return t * t * (3 - 2 * t)


def blood_splatter(rng, s):
    """A high-velocity splatter: dense core, tapered streaks radiating out, satellite droplets."""
    f = [0.0] * (s * s)
    c = s * 0.5
    # A main direction: real spatter is thrown one way (the bullet's), so it fans out off-centre.
    main = rng.random() * math.tau
    # Core: many small lumps, smeared along the main direction.
    for _ in range(rng.randint(18, 30)):
        a = main + rng.gauss(0, 0.9)
        d = rng.random() ** 1.3 * s * 0.13
        splat_disc(f, s, c + math.cos(a) * d, c + math.sin(a) * d, s * rng.uniform(0.025, 0.075), rng.uniform(0.7, 1.2))
    # A few wobbly, uneven streaks, mostly along the main direction.
    for _ in range(rng.randint(3, 7)):
        a = main + rng.gauss(0, 0.7)
        length = s * rng.uniform(0.1, 0.3)
        r0 = s * rng.uniform(0.008, 0.022)
        steps = int(length / max(1.0, r0 * 0.4)) + 1
        x, y = c + math.cos(a) * s * 0.05, c + math.sin(a) * s * 0.05
        for i in range(steps):
            t = i / float(steps)
            a += rng.gauss(0, 0.06)
            x += math.cos(a) * length / steps
            y += math.sin(a) * length / steps
            r = r0 * (1.0 - 0.7 * t) * rng.uniform(0.8, 1.2)
            splat_disc(f, s, x, y, max(1.2, r), 1.1)
        splat_disc(f, s, x, y, max(1.6, r0 * rng.uniform(0.6, 1.1)), 1.3)
    # Satellite droplets with tails pointing back at the source; denser toward the main direction.
    for _ in range(rng.randint(60, 110)):
        a = main + rng.gauss(0, 1.1)
        d = s * (0.1 + rng.random() ** 0.8 * 0.36)
        r = s * rng.uniform(0.004, 0.014) * (1.2 - d / (s * 0.5))
        r = max(1.3, r)
        x, y = c + math.cos(a) * d, c + math.sin(a) * d
        splat_disc(f, s, x, y, r, 1.4)
        tail = rng.randint(0, 4)
        for k in range(1, tail + 1):
            splat_disc(f, s, x - math.cos(a) * r * 0.9 * k, y - math.sin(a) * r * 0.9 * k, max(1.0, r * (1.0 - k / (tail + 1.5))), 1.2)
    # Fine mist specks everywhere.
    for _ in range(rng.randint(150, 260)):
        a = main + rng.gauss(0, 1.4)
        d = s * rng.uniform(0.05, 0.47)
        splat_disc(f, s, c + math.cos(a) * d, c + math.sin(a) * d, rng.uniform(0.9, 2.2), 1.2)
    return f


def blood_pool(rng, s):
    f = [0.0] * (s * s)
    c = s * 0.5
    for _ in range(rng.randint(10, 16)):
        a = rng.random() * math.tau
        d = rng.random() * s * 0.2
        splat_disc(f, s, c + math.cos(a) * d, c + math.sin(a) * d, s * rng.uniform(0.12, 0.24), 1.0)
    for _ in range(30):
        a = rng.random() * math.tau
        d = s * rng.uniform(0.3, 0.44)
        splat_disc(f, s, c + math.cos(a) * d, c + math.sin(a) * d, s * rng.uniform(0.006, 0.02), 1.3)
    return f


def shade_blood(field, noise, s, pixels, ox, oy, stride_px):
    """Coverage = threshold(field + noise); colour: near-black maroon in thick parts, brighter red at thin edges."""
    for y in range(s):
        o = y * s
        po = ((oy + y) * stride_px + ox) * 4
        for x in range(s):
            v = field[o + x]
            n = noise[o + x]
            cov = smoothstep(0.4, 0.5, v + (n - 0.5) * 0.5)
            thick = smoothstep(0.5, 2.6, v + (n - 0.5) * 0.8)
            # sRGB values: edge (150, 12, 10) -> core (52, 2, 3), clotted mottling.
            m = 0.7 + 0.6 * n
            r = (150 - 98 * thick) * m
            g = (12 - 10 * thick) * m
            b = (10 - 7 * thick) * m
            i = po + x * 4
            pixels[i] = max(0, min(255, int(r)))
            pixels[i + 1] = max(0, min(255, int(g)))
            pixels[i + 2] = max(0, min(255, int(b)))
            pixels[i + 3] = int(cov * 255)


def bullet_hole(rng, s, noise, pixels, ox, oy, stride_px):
    c = s * 0.5 + rng.uniform(-2, 2)
    hole = s * rng.uniform(0.06, 0.085)
    ring = s * rng.uniform(0.2, 0.3)
    cracks = []
    for _ in range(rng.randint(4, 8)):
        a = rng.random() * math.tau
        cracks.append((math.cos(a), math.sin(a), s * rng.uniform(0.25, 0.46), rng.uniform(0.6, 1.2)))
    for y in range(s):
        o = y * s
        po = ((oy + y) * stride_px + ox) * 4
        for x in range(s):
            dx = x - c
            dy = y - c
            d = math.sqrt(dx * dx + dy * dy) + 1e-4
            n = noise[o + x]
            dd = d + (n - 0.5) * s * 0.06
            # crushed / chipped ring
            ring_a = 1.0 - smoothstep(hole, ring, dd)
            ring_a *= 0.55 + 0.45 * n
            # hole core
            core = 1.0 - smoothstep(hole * 0.75, hole * 1.15, dd)
            # hairline cracks
            crack = 0.0
            for cx_, cy_, ln, wd in cracks:
                along = dx * cx_ + dy * cy_
                if 0 < along < ln:
                    perp = abs(dx * cy_ - dy * cx_ + math.sin(along * 0.21) * 1.5)
                    w = wd * (1.0 - along / ln) + 0.6
                    crack = max(crack, (1.0 - smoothstep(0.0, w, perp)) * (1.0 - along / ln * 0.7))
            a = max(core, ring_a * 0.85, crack * 0.8)
            # colour: black core, powdery light-grey ring rim (exposed concrete), dark cracks
            dust = smoothstep(hole * 1.1, ring * 0.9, dd) * ring_a
            lum = 12 + 120 * dust * (0.7 + 0.3 * n)
            if core > 0.5:
                lum = 6
            elif crack > ring_a:
                lum = 30
            i = po + x * 4
            pixels[i] = pixels[i + 1] = pixels[i + 2] = max(0, min(255, int(lum)))
            pixels[i + 3] = int(max(0.0, min(1.0, a)) * 255)


def puff(rng, s):
    nz = ValueNoise(rng, s, cells=(4, 9, 20), weights=(0.55, 0.3, 0.15)).field()
    px = bytearray(s * s * 4)
    c = (s - 1) * 0.5
    for y in range(s):
        for x in range(s):
            d = math.sqrt((x - c) ** 2 + (y - c) ** 2) / c
            n = nz[y * s + x]
            a = (1.0 - smoothstep(0.25, 1.0, d + (n - 0.5) * 0.5)) * (0.55 + 0.45 * n)
            i = (y * s + x) * 4
            lum = int(200 + 55 * n)
            px[i] = px[i + 1] = px[i + 2] = min(255, lum)
            px[i + 3] = int(max(0.0, min(1.0, a)) * 255)
    return px


def drop(s):
    px = bytearray(s * s * 4)
    c = (s - 1) * 0.5
    for y in range(s):
        for x in range(s):
            d = math.sqrt((x - c) ** 2 + (y - c) ** 2) / c
            a = 1.0 - smoothstep(0.7, 0.95, d)
            i = (y * s + x) * 4
            hl = 1.0 - smoothstep(0.0, 0.35, math.sqrt((x - c * 0.7) ** 2 + (y - c * 0.7) ** 2) / c)
            lum = int(170 + 85 * hl)
            px[i] = px[i + 1] = px[i + 2] = lum
            px[i + 3] = int(a * 255)
    return px


def main(out):
    os.makedirs(out, exist_ok=True)
    rng = random.Random(1337)

    s = 512
    atlas = bytearray((s * 2) * (s * 2) * 4)
    for k in range(4):
        f = blood_splatter(rng, s)
        n = ValueNoise(rng, s).field()
        shade_blood(f, n, s, atlas, (k % 2) * s, (k // 2) * s, s * 2)
        print("splat", k)
    write_png(os.path.join(out, "T_ZS_BloodSplat.png"), s * 2, s * 2, atlas)

    pool = bytearray(s * s * 4)
    shade_blood(blood_pool(rng, s), ValueNoise(rng, s).field(), s, pool, 0, 0, s)
    write_png(os.path.join(out, "T_ZS_BloodPool.png"), s, s, pool)
    print("pool")

    h = 256
    holes = bytearray((h * 2) * (h * 2) * 4)
    for k in range(4):
        bullet_hole(rng, h, ValueNoise(rng, h, cells=(8, 20, 48), weights=(0.5, 0.3, 0.2)).field(), holes, (k % 2) * h, (k // 2) * h, h * 2)
    write_png(os.path.join(out, "T_ZS_BulletHole.png"), h * 2, h * 2, holes)
    print("holes")

    write_png(os.path.join(out, "T_ZS_Puff.png"), 128, 128, puff(rng, 128))
    write_png(os.path.join(out, "T_ZS_Drop.png"), 64, 64, drop(64))
    print("done")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "fx_textures")
