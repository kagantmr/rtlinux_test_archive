import socket
import struct
import math

# CONFIGURATION
TCP_IP = "0.0.0.0" # Listen on all interfaces
TCP_PORT = 8080
FFT_SIZE = 64
PACKET_SIZE = FFT_SIZE * 8  # 64 complex_t * 8 bytes

def recvall(sock, n):
    """Helper to ensure we get exactly n bytes from the stream"""
    data = b''
    while len(data) < n:
        packet = sock.recv(n - len(data))
        if not packet:
            return None # Connection closed
        data += packet
    return data

server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
server_sock.bind((TCP_IP, TCP_PORT))
server_sock.listen(1)

print(f"TCP Server listening on {TCP_IP}:{TCP_PORT}...")

try:
    while True:
        print("Waiting for connection...")
        conn, addr = server_sock.accept()
        print(f"Connected by {addr}")
        
        try:
            while True:
                # Force wait for exactly one full frame
                data = recvall(conn, PACKET_SIZE)
                
                if not data:
                    break # Client disconnected
                
                # Unpack and Visualize
                floats = struct.unpack(f'{FFT_SIZE*2}f', data)
                
                # Simple visualization (First 5 bins)
                print("\033c", end="") # Clear terminal
                print(f"--- LIVE TCP STREAM FROM {addr} ---")
                for i in range(1, 16): 
                    mag = math.sqrt(floats[i*2]**2 + floats[i*2+1]**2)
                    bar = '#' * int(mag * 50)
                    print(f"Bin {i:02}: |{bar}")
                    
        except Exception as e:
            print(f"Error: {e}")
        finally:
            conn.close()
            print("Connection closed.")

except KeyboardInterrupt:
    print("\nServer stopped.")
    server_sock.close()