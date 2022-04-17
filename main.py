from xml.dom.expatbuilder import theDOMImplementation
import seri
import screens
from engine import get_device
from luma.core.render import canvas
import threading

data_temp=seri.dataset("coolant temp")
data_boost=seri.dataset("inmani pressure")
data_inj=seri.dataset("fuel injection")
data_deg=seri.dataset("cam angle")

def signal_communication():
    global data_temp
    global data_boost
    global data_inj
    global data_deg
    cnt_temp=10
    inj_ask=True
    while True:
        data_ask=[data_boost]
        if(cnt_temp==0):
            data_ask+=[data_temp]
            cnt_temp=10
        else:
            cnt_temp-=1
        if(inj_ask):
            data_ask+=[data_inj]
        else:
            data_ask+=[data_deg]
        inj_ask=not(inj_ask)
        seri.comm(0xa8,data_ask)

def show_screen():
    global data_temp
    global data_boost
    global data_inj
    global data_deg
    device = get_device()
    while True:
        with canvas(device) as draw:
            screens.primitives(draw,data_boost,data_temp,data_inj,data_deg)

t1=threading.Thread(target=signal_communication)
t2=threading.Thread(target=show_screen)

t1.start()
t2.start()
