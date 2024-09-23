import serial
import time

# to send from USB Serial Port (COM13)
ser = serial.Serial(
    port='COM13',             # Send data from CP2105 UART (USB Serial Port) to STM32 (COM13)
    baudrate=2000000,          # Baud rate
    parity=serial.PARITY_NONE, # No parity
    stopbits=serial.STOPBITS_ONE, # One stop bit
    bytesize=serial.EIGHTBITS, # Eight data bits
    timeout=1                 # Timeout in seconds
)

if not ser.is_open:
    ser.open()

def send_data(data):
    try:
        ser.write(data.encode())
        print(f"Data sent through COM13: {data}")
    except Exception as e:
        print(f"Error sending data through COM13: {e}")

data_to_send = "from STM32 ST-link"

counter = 1
while True:
    message = f"{counter} {data_to_send}"
    send_data(message)
    counter += 1
    if counter > 1000:
        counter = 1
    time.sleep(1)

# Close the serial port
ser.close()
