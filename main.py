import seri
import screens
from engine import get_device
from luma.core.render import canvas

data1=seri.dataset("coolant temp")
data2=seri.dataset("inmani pressure")
data3=seri.dataset("fuel injection")
data4=seri.dataset("cam angle")
device = get_device()
while True:
    out_data=seri.comm(0xa8, [data1,data2,data3,data4])
    out_data1=out_data[0].calc_data()
    out_data2=(out_data[1].calc_data()-101.3)/101.3
    out_data3=out_data[2].calc_data()
    out_data4=out_data[3].calc_data()
    print(out_data3)
    with canvas(device) as draw:
        screens.scr_main(out_data2,out_data1,out_data3,out_data4,draw)
