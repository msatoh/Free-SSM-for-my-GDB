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

from engine import get_device
from luma.core.render import canvas
from PIL import ImageFont
import math
import random
import collections
import statistics
from seri import dataset

temp_history=collections.deque([],78)
temp_lastcollected=0
boost_history=collections.deque([],16)

def primitives(draw,BOOST,temp,inj,deg):
    draw.text((47, -1), "Multi")
    draw.text((47,7),"Gauge")
    digit_font=ImageFont.truetype("digit.ttf",7)
    num_font=ImageFont.truetype("DejaVuSans.ttf",19)
    #deg
    current_inj=inj.calc_data()
    inj_depth=int(31875*current_inj/inj.MAX/32) if inj.MAX!=0 else 0
    draw.text((25, 0), str("deg"),font=digit_font)
    draw.pieslice((-64,0)+(64,128), 270, 270+deg.calc_data(), fill=(inj_depth,inj_depth,inj_depth))
    draw.pieslice((-64,0)+(64,128), 270, 315)
    # fuel inj
    draw.text((5, 59), str("current"),font=digit_font)
    draw.text((29,38),str('{:.1f}'.format(current_inj)),font=ImageFont.truetype("DejaVuSans.ttf",27))
    draw.text((47, 21), str("MAX inj."),font=digit_font)
    draw.text((46, 24), str('{:.1f}'.format(inj.MAX*32/125)),font=num_font)

    # turbo gauge
    draw.ellipse((192, 0, 255, 63))
    boost_history.appendleft(BOOST.calc_data())
    j=0
    for i in boost_history:
        angle=(i-101.3)*180/101.3
        tip_x=int(-29*math.sin(math.radians(angle))+224)
        tip_y=int(29*math.cos(math.radians(angle))+32)
        depth=255-int(j*17)
        draw.line((224, 32, tip_x, tip_y),fill=(depth,depth,depth))
        j+=1
    draw.text((213, 16), str("BOOST"),font=digit_font)
    draw.text((194, 32), str("0.5"),font=digit_font)
    draw.text((222, 57), str("0"),font=digit_font)
    draw.text((245, 32), str("1.5"),font=digit_font)
    draw.text((222, 3), str("1"),font=digit_font)

    # temp
    global temp_history
    global temp_lastcollected
    temp_current=int(temp.calc_data())
    if(temp.last_updated>temp_lastcollected):
        temp_lastcollected=temp.last_updated
        temp_history.appendleft(temp.calc_data())
    if temp_current>60:
        draw.polygon([(70+temp_current, (120-temp_current)/3), (70+temp_current, 57), (130, 57),(130,20)], outline="green", fill="green")
    draw.polygon([(190, 0), (190, 57), (130, 57),(130,20)])
    draw.text((130, 59), str("60"),font=digit_font)
    draw.text((185, 59), str("120"),font=digit_font)
    draw.text((172,34),"temp",font=digit_font)
    temp_width, temp_height = draw.textsize(text=str(temp_current)+"°C", font=num_font)
    draw.text((190-temp_width,38),str(temp_current)+"°C",font=num_font)
    draw.rectangle((88, -1, 128, 63))
    draw.text((130,0),str("log"),font=digit_font)
    i=1
    for j in temp_history:
        draw.point((128-(i/2),120-j))
        i+=1

if __name__ == "__main__": #testbench
    try:
        device=get_device()
        in_boost=collections.deque([],3)
        in_temp=collections.deque([],10)
        in_inj=collections.deque([],1)
        in_deg=collections.deque([],5)
        data_boost=dataset("inmani pressure")
        data_temp=dataset("coolant temp")
        data_inj=dataset("fuel injection")
        data_deg=dataset("cam angle")
        for _ in range(100):
            with canvas(device) as draw:
                in_boost.append(random.randint(0,255))
                in_temp.append((random.randint(0,255)
                +random.randint(0,510)
                +random.randint(0,510)
                +random.randint(0,255)
                +random.randint(0,255)
                +random.randint(0,255))/8)
                in_inj.append(random.randint(0,255))
                in_deg.append(random.randint(0,255))
                data_boost.input_data(statistics.mean(in_boost))
                data_temp.input_data(statistics.mean(in_temp))
                data_inj.input_data(statistics.mean(in_inj))
                data_deg.input_data(statistics.mean(in_deg))
                primitives(draw,
                    data_boost,
                    data_temp,
                    data_inj,
                    data_deg
                    )
    except KeyboardInterrupt:
        pass
