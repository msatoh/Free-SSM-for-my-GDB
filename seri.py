import serial
import struct
import time

TIME_OUT=1

class dataset:
    dataid=[]
    unit=""
    def __init__(self, nm):
        if nm=="coolant temp":
            dataid=[0x00, 0x00, 0x08]
            unit="℃"
        elif nm=="inmani pressure":
            dataid=[0x00, 0x00, 0x0d]
            unit="kPa"
        elif nm=="throttle sensor":
            dataid=[0x00, 0x00, 0x15]
            unit="%"
    def definition( nm):
        if nm=="coolant temp":
            dataid=[0x00, 0x00, 0x08]
            unit="℃"
        elif nm=="inmani pressure":
            dataid=[0x00, 0x00, 0x0d]
            unit="kPa"
        elif nm=="throttle sensor":
            dataid=[0x00, 0x00, 0x15]
            unit="%"
    def return_data(data):
        if dataid==[0x00, 0x00, 0x08]:
            return data-40
        elif dataid==[0x00, 0x00, 0x0d]:
            return data
        elif dataid==[0x00, 0x00, 0x15]:
            return data*100/255


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
    if not(message in ["format error.","check sum error.","timed out."]):
        if message[4]==0xe8:
            lengthdata=len(data)
            conv_data=[]
#            print(message[3])
            if message[3]>=lengthdata+1:
                for i in range(lengthdata):
                    conv_data.append(data[i])
                    conv_data.append(message[i+5])
                print(conv_data)
            else:
                print("number of data mismatched.")
        else:
            print("inapropriate responce.")
    else:
        print(message)

ser = serial.Serial(
    port = "/dev/ttyUSB0",
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
#        for i in range(len(data)):
#            comp_data=comp_data+data[i].dataid
        send_data([sid]+[0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x15])
        receive_data(sid,data)
    else:
        send_data(data)
        print(receive_msg())

if __name__=="__main__":
    data1=dataset.definition("coolant temp")
    data2=dataset.definition("inmani pressure")
    while True:
        comm(0xa8, [data1,data2])
