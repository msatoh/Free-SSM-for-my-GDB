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
import random
import collections
import statistics

MAX_inj=0
temp_history=collections.deque([],78)

def primitives(device, draw,BOOST,temp,current_inj,deg):
    global MAX_inj
    draw.text((47, -1), "Multi")
    draw.text((47,7),"Gauge")
    digit_font=ImageFont.truetype("digit.ttf",7)
    num_font=ImageFont.truetype("DejaVuSans.ttf",19)
    # turbo gauge
    draw.ellipse((192, 0, 255, 63), outline="white", fill="black")
    angle=BOOST*180
    tip_x=int(29*math.cos(math.radians(90+angle))+224)
    tip_y=int(29*math.sin(math.radians(90+angle))+32)
    draw.line((224, 32, tip_x, tip_y), fill="white")
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
    if MAX_inj<current_inj:
        MAX_inj=current_inj
    current_char_width, current_char_height = draw.textsize(text="temp", font=digit_font)
    current_digit_width, current_digit_height = draw.textsize(text=str(current_inj), font=ImageFont.truetype("DejaVuSans.ttf",27))
    draw.text((5, 59), str("current"),font=digit_font)
    draw.text((current_char_width+11,63-current_digit_height),str('{:.1f}'.format(current_inj)),font=ImageFont.truetype("DejaVuSans.ttf",27))
    draw.text((47, 21), str("MAX inj."),font=digit_font)
    MAX_digit_width, MAX_digit_height = draw.textsize(text="temp", font=num_font)
    draw.text((96-MAX_digit_width, 24), str('{:.1f}'.format(MAX_inj)),font=num_font)
    #deg
    inj_depth=int(255*current_inj/MAX_inj)
    draw.text((25, 0), str("deg"),font=digit_font)
    draw.pieslice((-64,0)+(64,128), 270, 270+deg, fill=(inj_depth,inj_depth,inj_depth))
    draw.pieslice((-64,0)+(64,128), 270, 315, outline="white")

    # temp
    # Draw a triangle.
    global temp_history
    temp_history.appendleft(temp)
    if temp>60:
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
    i=1
    for j in temp_history:
        draw.point((128-(i/2),120-j),"white")
        i+=1

def sc(device):
    print("Testing basic canvas graphics...")
    in_boost=collections.deque([],3)
    in_temp=collections.deque([],3)
    in_inj=collections.deque([],3)
    in_deg=collections.deque([],3)
    for _ in range(100):
        with canvas(device) as draw:
            in_boost.append(random.uniform(0,2))
            in_temp.append(random.randint(0,120))
            in_inj.append(random.uniform(0,21.5))
            in_deg.append(random.randint(0,45))
            primitives(device, draw,
                statistics.mean(in_boost),
                int(statistics.mean(in_temp)),
                statistics.mean(in_inj),
                int(statistics.mean(in_deg))
                )
            time.sleep(0.01)

def main():
    device = get_device()
    font = ImageFont.truetype("BRZimpFont.ttf", 27)
    text="welcome"
    with canvas(device) as draw:
        char_width, char_height = draw.textsize(text=text, font=font)
        draw.text(((256 - char_width) / 2,(56 - char_height) / 2), text=text,font=font)#, fill="white")
    time.sleep(1)

    sc(device)

    print("Testing contrast (dim/bright cycles)...")
    for level in range(0, 255, 5):
        device.contrast(level)
        time.sleep(0.1)

    print("Testing display ON/OFF...")
    time.sleep(0.4)
    device.hide()
    time.sleep(0.4)
    device.show()
    time.sleep(0.2)
    device.clear()

    print("Testing screen updates...")
    for x in range(200):
        with canvas(device) as draw:
            now = datetime.datetime.now()
            draw.text((x, 4), str(now.date()), fill="white")
            draw.text((10, 16), str(now.time()), fill="white")
            time.sleep(0.01)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass