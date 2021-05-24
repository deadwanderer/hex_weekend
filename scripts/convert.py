#!/usr/bin/env python

# Will split a png cubemap/skymap image produced by blender into 6 separated image files for use in a skybox within unity
# Requires Python Imaging Library > http://www.pythonware.com/products/pil/

# The author takes no responsibility for any damage this script might cause,
# feel free to use, change and or share this script.
# 2013-07, CanOfColliders, m@canofcolliders.com

from PIL import Image
import sys
import os

path = os.path.abspath("") + "/"
processed = False


def processImage(path, name):
    imgPath = os.path.join(path, name)
    print(imgPath)
    img = Image.open(os.path.join(path, name))
    # splits the width of the image by 3, expecting the 3x2 layout blender produces.
    width = img.size[0] / 4
    height = img.size[1] / 3
    splitAndSave(img, 0, height, width, addToFilename(name, "_left"))
    splitAndSave(img, width, height, width, addToFilename(name, "_front"))
    splitAndSave(img, width * 2, height, width, addToFilename(name, "_right"))
    splitAndSave(img, width*3, height, width, addToFilename(name, "_back"))
    splitAndSave(img, width, height * 2, width, addToFilename(name, "_down"))
    splitAndSave(img, width, 0, width, addToFilename(name, "_up"))


def addToFilename(name, add):
    name = name.split('.')
    return name[0] + add + "." + name[1]


def splitAndSave(img, startX, startY, size, name):
    area = (startX, startY, startX + size, startY + size)
    saveImage(img.crop(area), path, name)


def saveImage(img, path, name):
    try:
        img.save(os.path.join(path, name))
    except:
        print("*   ERROR: Could not convert image.")
        pass


for arg in sys.argv:
    if ".png" in arg or ".jpg" in arg:
        processImage(path, arg)
        processed = True

if not processed:
    print("*  ERROR: No Image")
    print("   usage: 'python script.py image-name.png'")
