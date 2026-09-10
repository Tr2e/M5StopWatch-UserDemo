import hashlib,pathlib,subprocess,sys,time,serial
from serial.tools import list_ports
port='/dev/cu.usbmodem83401'
firmware=pathlib.Path('/Users/xudanyang/Documents/stopwatch/userdemo/build/StopWatch-UserDemo.bin')
expected,prefix,duration=sys.argv[1:]
def verify():
    ports=list(list_ports.comports())
    if not any(p.device==port and p.vid==0x303a and p.pid==0x1001 and (p.serial_number or '').upper()=='44:1B:F6:C1:8A:00' for p in ports):sys.exit('StopWatch USB identity mismatch; refusing serial access')
verify()
actual=hashlib.sha256(firmware.read_bytes()).hexdigest()
if actual!=expected:sys.exit('Firmware hash changed; refusing flash')
print('Verified StopWatch 83401 / 303A:1001 / 44:1B:F6:C1:8A:00; firmware',actual,flush=True)
log=pathlib.Path(prefix+'-flash.log')
with log.open('w') as out:
    result=subprocess.run([sys.executable,'-m','esptool','--chip','esp32s3','--port',port,'--baud','460800','--before','default_reset','--after','hard_reset','write_flash','--flash_mode','dio','--flash_freq','80m','--flash_size','16MB','0x20000',str(firmware)],stdout=out,stderr=subprocess.STDOUT)
print('\n'.join(log.read_text().splitlines()[-8:]),flush=True)
if result.returncode:sys.exit(result.returncode)
verify()
c=serial.Serial(port=None,baudrate=115200,timeout=.2);c.dtr=False;c.rts=False;c.port=port;c.open()
end=time.monotonic()+int(duration);pending=''
try:
    with pathlib.Path(prefix+'-device.log').open('w') as out:
        while time.monotonic()<end:
            text=c.read(c.in_waiting or 1).decode(errors='replace');out.write(text);out.flush();pending+=text
            while '\n' in pending:
                line,pending=pending.split('\n',1)
                if any(tag in line for tag in ('InspectionAB','GarageMemory','RacerMemory','ELF file SHA','[Launcher] on open')):print(line,flush=True)
                if '[InspectionAB]' in line and 'END mismatches=' in line:end=time.monotonic()+1
finally:c.close()
