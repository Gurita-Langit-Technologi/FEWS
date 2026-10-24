import sys
import sx126x
import time
import threading
import termios
import tty

# Save terminal settings to restore them later
old_settings = termios.tcgetattr(sys.stdin)
tty.setcbreak(sys.stdin.fileno())

node = sx126x.sx126x(serial_num="/dev/ttyS0", freq=929, addr=1, power=22, rssi=False)

node_lock = threading.Lock()

def sender():
    with node_lock:
        node.set_mode("normal")
    print("[Node 0]: Sender ready\n")
    
    n = 0
    while True:
        n += 1
        msg = f"CALL:88{n}"
        with node_lock:
            node.send(msg)
        print(f"[Node 0]: SENT {msg}")
        time.sleep(15)

def receiver():
    with node_lock:
        node.set_mode("normal")
    
    print("[Node 0]: Receiver ready\n")
    while True:
        with node_lock:
            node.receive()
    

threading.Thread(target=sender, daemon=True).start()
threading.Thread(target=receiver, daemon=True).start()

try:
    print("Press Ctrl+C to exit")

    n=0
    while True:
        n=n+1
        print(f"Period {n}")
        time.sleep(100)
        
        
except KeyboardInterrupt:
    print("\nExiting...")
except Exception as e:
    print(f"\nAn error occurred: {e}")
finally:
    # Always ensure terminal settings are restored cleanly
    termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)
