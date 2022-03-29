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
from tkinter import Image
from engine import get_device
from luma.core.render import canvas
from PIL import ImageFont
import math


def primitives(device, draw):
    draw.text((47, -1), "Multi")
    draw.text((47,7),"Gauge")
    # turbo gauge
    # Draw an ellipse.
    draw.ellipse((192, 0, 255, 63), outline="white", fill="black")
    # Draw a line.
    BOOST=0.7
    angle=BOOST*180
    tip_x=int(29*math.cos(math.radians(90+angle))+224)
    tip_y=int(29*math.sin(math.radians(90+angle))+32)
    draw.line((224, 32, tip_x, tip_y), fill="white")
    # Write digits
    digit_font=ImageFont.truetype("digit.ttf",7)
    BOOST_char_width, BOOST_char_height = draw.textsize(text="BOOST", font=digit_font)
    O_char_width, O_char_height = draw.textsize(text="0", font=digit_font)
    one_char_width, one_char_height = draw.textsize(text="1", font=digit_font)
    two_char_width, two_char_height = draw.textsize(text="1.5", font=digit_font)
    draw.text((224-BOOST_char_width/2, 16), str("BOOST"),font=digit_font, fill="white")
    draw.text((194, 32), str("0.5"),font=digit_font, fill="white")
    draw.text((225-O_char_width/2, 62-O_char_height), str("0"),font=digit_font, fill="white")
    draw.text((255-two_char_width, 32), str("1.5"),font=digit_font, fill="white")
    draw.text((225-one_char_width/2, 3), str("1"),font=digit_font, fill="white")
    # fuel inj
    # Draw a rectangle.
    MAX_inj=21.5
    current_inj=8.6
    num_font=ImageFont.truetype("DejaVuSans.ttf",19)
    current_char_width, current_char_height = draw.textsize(text="temp", font=digit_font)
    current_digit_width, current_digit_height = draw.textsize(text=str(current_inj), font=ImageFont.truetype("DejaVuSans.ttf",27))
    draw.text((5, 59), str("current"),font=digit_font)
    draw.text((current_char_width+11,63-current_digit_height),str(current_inj),font=ImageFont.truetype("DejaVuSans.ttf",27))
    draw.text((47, 21), str("MAX inj."),font=digit_font)
    MAX_digit_width, MAX_digit_height = draw.textsize(text="temp", font=num_font)
    draw.text((96-MAX_digit_width, 24), str(MAX_inj),font=num_font)
    #deg
    deg=22
    inj_depth=int(255*current_inj/MAX_inj)
    draw.text((25, 0), str("deg"),font=digit_font)
    draw.pieslice((-64,0)+(64,128), 270, 270+deg, fill=(inj_depth,inj_depth,inj_depth))
    draw.pieslice((-64,0)+(64,128), 270, 315, outline="white")

    # temp
    # Draw a triangle.
    temp=96
    draw.polygon([(70+temp, (120-temp)/3), (70+temp, 57), (130, 57),(130,20)], outline="green", fill="green")
    draw.polygon([(190, 0), (190, 57), (130, 57),(130,20)], outline="white")
    draw.text((130, 59), str("60"),font=digit_font, fill="white")
    draw.text((185, 59), str("120"),font=digit_font, fill="white")
    temp_char_width, temp_char_height = draw.textsize(text="temp", font=digit_font)
    draw.text((190-temp_char_width,41-temp_char_height),"temp",font=digit_font)
    digit_char_width, digit_char_height = draw.textsize(text=str(temp)+"℃", font=num_font)
    draw.text((190-digit_char_width,56-digit_char_height),str(temp)+"℃",font=num_font)
    draw.rectangle((88, -1, 128, 63), outline="white", fill="black")
    draw.text((130,0),str("log"),font=digit_font)

def main():
    device = get_device()
    font = ImageFont.truetype("BRZimpFont.ttf", 27)
    text="welcome"
    with canvas(device) as draw:
        char_width, char_height = draw.textsize(text=text, font=font)
        draw.text(((256 - char_width) / 2,(56 - char_height) / 2), text=text,font=font)#, fill="white")
    time.sleep(1)

    print("Testing basic canvas graphics...")
    #for _ in range(2):
    with canvas(device) as draw:
        primitives(device, draw)
    time.sleep(2.1)

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
    time.sleep(0.4)
    device.hide()

    time.sleep(0.4)
    device.show()

    print("Testing clear display...")
    time.sleep(1)
    device.clear()

    print("Testing screen updates...")
    time.sleep(0.8)
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