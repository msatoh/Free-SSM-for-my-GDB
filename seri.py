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
    def calc_data(self,data):
        if self.dataid==[0x00, 0x00, 0x08]:
            return data-40
        elif self.dataid==[0x00, 0x00, 0x0d]:
            return data
        elif self.dataid==[0x00, 0x00, 0x15]:
            return data*100/255
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
            print("tx: ",buf)
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
    cs=[(128+16+240+length+sum(data))%256]
    send_msg([0x80, 0x10, 0xf0]+[length]+data+cs)

def receive_msg():
    mes_list=[]
    begin=time.time()
    while time.time()-begin < TIME_OUT or len(mes_list)==0:
        rx_data = ser.read()
        a = struct.unpack("B",rx_data)
        mes_list.append(a[0])
        if len(mes_list)==1:
            if mes_list[0]==128:
                begin=time.time()
            else:
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
        "check sum error.",
        "timed out."
        ]):
        if message[4]==0xe8:
            datalength=len(data)
            if message[3]==datalength+1:
                for i in range(datalength):
                    data[i].input_data(message[i+5])
                    print(data[i].item,"=",data[i].calc_data(data[i].nowdata),data[i].unit," time:",data[i].last_updated,end="ms ")
                print("\n")
            else:
                print("number of data mismatched.")
        else:
            print("inapropriate responce.")
    else:
        print(message)

ser = serial.Serial(
    port = "/dev/ttyUSB*",
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
        for i in data:
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
