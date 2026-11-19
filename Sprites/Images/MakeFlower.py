#Makes Flower.png, the flower plant at the bottom right of Fighting320.png, which sprites draws in front of Maki: the
#plant's pixels with the rest transparent, cut to the plant.  The flower is its red and orange petals and its green
#leaf, inside an outline drawn round it; the stem and the leaves under it are the greens in a column below it down to
#the picture's bottom, a guess, as they are hard to tell from the grass there.  It prints where its top left is in Fighting320.png, which Sprites.cpp has.
#Run it from this directory: python3 MakeFlower.py
from PIL import Image, ImageDraw

OUTLINE = [(278, 163), (282, 157), (287, 154), (290, 152), (296, 153), (302, 154), (306, 155), (309, 151), (315, 151),
		   (316, 160), (315, 168), (314, 177), (310, 180), (304, 178), (298, 178), (290, 178), (283, 176), (277, 170)]


STEM = [(296, 175), (303, 173), (313, 173), (315, 178), (315, 200), (294, 200), (294, 180)]


def IsStem(r, g, b):
	return (g > r + 10) and (g >= b)	#Green, the dark stem too.


def IsPlant(r, g, b):
	if (r > 150) and (r > g + 30):
		return True		#A petal: red, pink or orange.
	if (g > 120) and (g > r + 25) and (g > b + 10):
		return True		#The leaf: bright green.
	return False


source = Image.open('Fighting320.png').convert('RGB')
pixels = source.load()
inside = Image.new('L', source.size, 0)
ImageDraw.Draw(inside).polygon(OUTLINE, fill=255)
ImageDraw.Draw(inside).polygon(STEM, fill=128)
insidePixels = inside.load()
width, height = source.size

plant = set()
for y in range(height):
	for x in range(width):
		if (insidePixels[x, y] == 255) and IsPlant(*pixels[x, y]):
			plant.add((x, y))
		elif (insidePixels[x, y] == 128) and IsStem(*pixels[x, y]):
			plant.add((x, y))

#A pixel left out with the plant on three or four sides is a hole in it.
for passes in range(2):
	holes = set()
	for y in range(height):
		for x in range(width):
			if insidePixels[x, y] and ((x, y) not in plant):
				sides = sum(((x + dx, y + dy) in plant) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)))
				if sides >= 3:
					holes.add((x, y))
	plant |= holes

#A pixel with no more than one of the plant's beside it (of eight) is a speck of something else.
for passes in range(2):
	specks = set()
	for x, y in plant:
		near = sum(((x + dx, y + dy) in plant) for dx in (-1, 0, 1) for dy in (-1, 0, 1) if (dx or dy))
		if near <= 1:
			specks.add((x, y))
	plant -= specks

left = min(x for x, y in plant)
top = min(y for x, y in plant)
right = max(x for x, y in plant) + 1
bottom = max(y for x, y in plant) + 1
flower = Image.new('RGBA', (right - left, bottom - top), (0, 0, 0, 0))
for x, y in plant:
	r, g, b = pixels[x, y]
	flower.putpixel((x - left, y - top), (r, g, b, 255))
flower.save('Flower.png')
print('Flower.png: %d by %d at (%d, %d), %d pixels' % (right - left, bottom - top, left, top, len(plant)))
