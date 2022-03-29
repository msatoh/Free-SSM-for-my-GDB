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
import math


def primitives(device, draw):
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
    digit_font=ImageFont.truetype("digit.ttf",7)#("SNchibi2_5.TTF", 5)
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
    draw.rectangle((0, -1, 45, 63), outline="white", fill="black")
    draw.text((47, 59), str("0"),font=digit_font)
    draw.text((47, 0), str("21.5"),font=digit_font)
    draw.text((47, 7), str("MAX"),font=digit_font)
    draw.text((47, 14), str("inj."),font=digit_font)
    # temp
    # Draw a triangle.
    temp=96
    draw.polygon([(60+temp, 1+(120-temp)/3), (60+temp, 57), (120, 57),(120,20)], outline="green", fill="green")
    draw.polygon([(190, 0), (190, 57), (120, 57),(120,20)], outline="white")
    draw.text((120, 59), str("60"),font=digit_font, fill="white")
    draw.text((185, 59), str("120"),font=digit_font, fill="white")
    temp_char_width, temp_char_height = draw.textsize(text="temp", font=digit_font)#ImageFont.truetype("C&C Red Alert [INET].ttf",13))
    draw.text((190-temp_char_width,35-temp_char_height),str("temp"),font=digit_font)#ImageFont.truetype("C&C Red Alert [INET].ttf",13))
    digit_char_width, digit_char_height = draw.textsize(text=str(temp)+"℃", font=ImageFont.truetype("DejaVuSans.ttf",28))
    draw.text((190-digit_char_width,56-digit_char_height),str(temp)+"℃",font=ImageFont.truetype("DejaVuSans.ttf",28))

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