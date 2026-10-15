"""Draws the port's icon (embers rising in a smoke spiral on a night asphalt field) as .ico and
.png. An original drawing: no game artwork or logo. Needs Pillow.
usage: make_icon.py launcher/issn.ico launcher/issn.png"""
import math
import sys
from PIL import Image, ImageDraw, ImageFilter

S = 1024  # drawn large, scaled down for every icon size


def smoke_path(t0, t1, turns, phase, steps=400):
    """Points of a rising spiral: from the bottom centre up, widening, turns times around."""
    points = []
    for i in range(steps + 1):
        t = t0 + (t1 - t0) * i / steps
        y = 860 - 700 * t
        radius = 40 + 230 * t ** 1.2
        angle = phase + turns * 2 * math.pi * t
        points.append((512 + radius * math.sin(angle), y - 40 * math.cos(angle) * t))
    return points


def stroke(draw, points, width0, width1, color):
    for i in range(len(points) - 1):
        f = i / (len(points) - 1)
        w = int(width0 + (width1 - width0) * f)
        draw.line((points[i], points[i + 1]), fill=color, width=max(w, 1))
        draw.ellipse((points[i][0] - w / 2, points[i][1] - w / 2, points[i][0] + w / 2, points[i][1] + w / 2),
                     fill=color)


def draw():
    image = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    mask = Image.new('L', (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle((24, 24, S - 24, S - 24), radius=190, fill=255)

    field = Image.new('RGBA', (S, S))
    fd = ImageDraw.Draw(field)
    for y in range(S):
        t = y / S
        fd.line((0, y, S, y), fill=(int(18 + 10 * t), int(20 + 6 * t), int(26 - 4 * t), 255))

    # The glow under the smoke.
    glow = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(glow).ellipse((262, 560, 762, 980), fill=(255, 90, 30, 150))
    field.alpha_composite(glow.filter(ImageFilter.GaussianBlur(110)))

    # Smoke: two soft grey spirals, blurred.
    smoke = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    sd = ImageDraw.Draw(smoke)
    stroke(sd, smoke_path(0.0, 1.0, 1.25, 0.0), 120, 230, (160, 160, 168, 170))
    stroke(sd, smoke_path(0.08, 0.95, 1.25, math.pi), 90, 170, (115, 117, 126, 150))
    field.alpha_composite(smoke.filter(ImageFilter.GaussianBlur(26)))

    # Ember core: a bright orange spiral inside the smoke, then its hot centre line.
    ember = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    ed = ImageDraw.Draw(ember)
    stroke(ed, smoke_path(0.0, 0.92, 1.25, 0.0), 120, 50, (255, 96, 30, 255))
    field.alpha_composite(ember.filter(ImageFilter.GaussianBlur(22)))
    field.alpha_composite(ember.filter(ImageFilter.GaussianBlur(6)))
    core = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    stroke(ImageDraw.Draw(core), smoke_path(0.0, 0.88, 1.25, 0.0), 54, 16, (255, 220, 160, 255))
    field.alpha_composite(core.filter(ImageFilter.GaussianBlur(3)))

    # Sparks.
    sparks = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    kd = ImageDraw.Draw(sparks)
    for x, y, r in ((290, 290, 20), (740, 240, 17), (680, 430, 14), (330, 500, 12), (560, 150, 15), (790, 540, 11)):
        kd.ellipse((x - r, y - r, x + r, y + r), fill=(255, 150, 70, 255))
    field.alpha_composite(sparks.filter(ImageFilter.GaussianBlur(4)))
    field.alpha_composite(sparks)

    image.paste(field, (0, 0), mask)
    border = ImageDraw.Draw(image)
    border.rounded_rectangle((24, 24, S - 24, S - 24), radius=190, outline=(212, 70, 27, 255), width=22)
    return image


if __name__ == '__main__':
    big = draw()
    big.resize((256, 256), Image.LANCZOS).save(
        sys.argv[1], sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    big.resize((128, 128), Image.LANCZOS).save(sys.argv[2])
