#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright (c) 2014-18 Richard Hull and contributors
# See LICENSE.rst for details.
# PYTHON_ARGCOMPLETE_OK

"""
Use misc draw commands to create a simple image.

Ported from:
https://github.com/adafruit/Adafruit_Python_SSD1306/blob/master/examples/shapes.py
"""

import time
import datetime
from engine import get_device
from luma.core.render import canvas
from PIL import ImageFont


def primitives(device, draw):
    # Draw some shapes.
    # First define some constants to allow easy resizing of shapes.
    shape_width = 20
    # Draw an ellipse.
    draw.ellipse((191, 0, 255, 63), outline="white", fill="black")
    # Draw a rectangle.
    draw.rectangle((0, -1, 67, 63), outline="white", fill="black")
    # Draw a triangle.
    draw.polygon([(189, 0), (189, 57), (74, 57)], outline="white", fill="green")
    x =30
    # Draw a line.
    draw.line((x, 2, x + shape_width, 61), fill="white")
    # Write digits
    disp_font=ImageFont.truetype("SNchibi2_5.TTF", 5)
    draw.text((69, 59), str("0"),font=disp_font, fill="white")
    draw.text((69, 0), str("21.5"),font=disp_font, fill="white")
    draw.text((74, 59), str("20"),font=disp_font, fill="white")
    temp_char_width, temp_char_height = draw.textsize(text="110", disp_font)
    draw.text((191-temp_char_width, 59), str("110"),font=disp_font, fill="white")

def main():
    device = get_device()
    font = ImageFont.truetype("BRZimpFont.ttf", 27)
    text="welcome"
    with canvas(device) as draw:
        char_width, char_height = draw.textsize(text=text, font=font)
        draw.text(((256 - char_width) / 2,(56 - char_height) / 2), text=text,font=font)#, fill="white")
    time.sleep(2)

    print("Testing basic canvas graphics...")
    for _ in range(2):
        with canvas(device) as draw:
            primitives(device, draw)
    time.sleep(1.1)
    print("Testing contrast (dim/bright cycles)...")
    for level in range(255, -1, -10):
        device.contrast(level)
        time.sleep(0.1)
    time.sleep(0.5)

    for level in range(0, 255, 10):
        device.contrast(level)
        time.sleep(0.1)

    time.sleep(1)

    print("Testing display ON/OFF...")
    time.sleep(0.5)
    device.hide()

    time.sleep(0.5)
    device.show()

    print("Testing clear display...")
    time.sleep(2)
    device.clear()

    print("Testing screen updates...")
    time.sleep(2)
    for x in range(40):
        with canvas(device) as draw:
            now = datetime.datetime.now()
            draw.text((x, 4), str(now.date()), fill="white")
            draw.text((10, 16), str(now.time()), fill="white")
            time.sleep(0.1)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass