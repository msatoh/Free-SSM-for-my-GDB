import seri
import screens
from engine import get_device
from luma.core.render import canvas

data_temp=seri.dataset("coolant temp")
data_boost=seri.dataset("inmani pressure")
data_inj=seri.dataset("fuel injection")
data_deg=seri.dataset("cam angle")
device = get_device()
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
    with canvas(device) as draw:
        screens.primitives(data_boost,data_temp,data_inj,data_deg)
