import serial
import struct
import time

TIME_OUT=1

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
    cs=[(128+16+240+len(data)+sum(data))%256]
    send_msg([0x80, 0x10, 0xf0]+[len(data)]+data+cs)

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
#        elif len(mes_list)==3:
#            if not(mes_list[1]==240 and mes_list[2]==16):
#                mes_list=[]
        elif len(mes_list)==4:
            mes_len=mes_list[3]
        elif len(mes_list)>4:
            if mes_len==0:
                if mes_list[len(mes_list)-1]==sum(mes_list[0:len(mes_list)-1])%256:
                    if (mes_list[1]==240 and mes_list[2]==16):
                        return mes_list
                    else:
                        mes_list=[]
                else:
                    return "check sum error."
            mes_len-=1
    return "timed out."

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

if __name__=="__main__":
    while True:
        send_data([0xbf])
        print("rx: ",receive_msg())