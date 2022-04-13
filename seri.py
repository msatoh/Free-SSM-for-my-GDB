import serial
import struct
import time

TIME_OUT=1

class dataset:
    dataid=[]
    item=""
    unit=""
    nowdata=0x00
    MAX=0x00
    min=0xff
    last_updated=0
        
    def __init__(self, nm):
        self.item=nm
        if nm=="coolant temp":
            self.dataid=[0x00, 0x00, 0x08]
            self.unit="℃"
        elif nm=="inmani pressure":
            self.dataid=[0x00, 0x00, 0x0d]
            self.unit="kPa"
        elif nm=="throttle sensor":
            self.dataid=[0x00, 0x00, 0x15]
            self.unit="%"
        elif nm=="fuel injection":
            self.dataid=[0x00,0x00,0x3B]
            self.unit="%"
        elif nm=="cam angle":
            self.dataid=[0x00,0x00,0x28]
            self.unit="°"
        elif nm=="inmani temp":
            self.dataid=[0x00,0x00,0x12]
            self.unit="℃"
    def calc_data(self):
        if self.dataid==[0x00, 0x00, 0x08]:
            return self.nowdata-40
        elif self.dataid==[0x00, 0x00, 0x0d]:
            return self.nowdata
        elif self.dataid==[0x00, 0x00, 0x15]:
            return self.nowdata*100/255
        elif self.dataid==[0x00,0x00,0x3B]:
            return self.nowdata*100/255
        elif self.dataid==[0x00,0x00,0x28]:
            return (self.nowdata-128)/2
        elif self.dataid==[0x00,0x00,0x12]:
            return self.nowdata-40
    def input_data(self,data):
        self.nowdata=data
        self.last_updated=time.time()
        if self.nowdata>self.MAX:
            self.MAX=self.nowdata
        if self.nowdata<self.min:
            self.min=self.nowdata


def send_msg( buf ):
    sent=False
    begin=time.time()
    while ser.out_waiting>=0:
        if not(sent):
            for b in buf:
                a = struct.pack( "B", b )
                ser.write(a)
            ser.flush()
            print("tx:",buf)
            sent=True
            if ser.out_waiting == 0:
                break
            if time.time()-begin>TIME_OUT:
                print("transmission interrupted by timing out.")
                break
        else:
            break

def send_data(data):
    length=len(data)
    cs=[(384+length+sum(data))%256]#384=128+16+240
    send_msg([0x80, 0x10, 0xf0]+[length]+data+cs)

def receive_msg():
    mes_list=[]
    while True:
        rx_data = ser.read()
        a = struct.unpack("B",rx_data)
        mes_list.append(a[0])
        if len(mes_list)==1:
            if mes_list[0]!=128:
                return "format error."
        elif len(mes_list)==4:
            mes_len=mes_list[3]
        elif len(mes_list)>4:
            if mes_len==0:
                if mes_list[-1]==sum(mes_list[0:-1])%256:
                    if (mes_list[1]==240 and mes_list[2]==16):
                        return mes_list
                    else:
                        mes_list=[]
                else:
                    return "check sum error."
            mes_len-=1
    return "timed out."

def receive_data(sid,data):
    message=receive_msg()
    if not(message in [
        "format error.",
        "check sum error."
        ]):
        if message[4]==0xe8:
            datalength=len(data)
            if message[3]==datalength+1:
                for i in range(datalength):
                    data[i].input_data(message[i+5])
                    print(data[i].item,"=",data[i].calc_data(),data[i].unit)
                print("time:",data[i].last_updated,"ms")
                return data
            else:
                print("number of data mismatched.")
        else:
            print("inapropriate responce.")
    else:
        print(message)

ser = serial.Serial(
    port = "/dev/serial0",
    baudrate = 4800,
    #parity = serial.PARITY_NONE,
    #bytesize = serial.EIGHTBITS,
    #stopbits = serial.STOPBITS_ONE,
    #timeout = None,
    #xonxoff = 0,
    #rtscts = 0,
    )

def comm(sid, data):
    if sid==0xa8:
        comp_data=[0x00]
        for i in range(len(data)):
            comp_data=comp_data+data[i].dataid
        send_data([sid]+comp_data)
        receive_data(sid,data)
    else:
        send_data(data)
        print(receive_msg())

if __name__=="__main__":
    data1=dataset("coolant temp")
    data2=dataset("inmani pressure")
    while True:
        comm(0xa8, [data1,data2])
