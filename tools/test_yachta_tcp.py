#!/usr/bin/env python3
import socket
import time

def test_tcp_connection():
    """Test TCP connection to Yachta sensor"""
    host = "192.168.4.1"
    port = 6666
    
    print(f"Testing TCP connection to {host}:{port}")
    
    try:
        # Create socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)  # 5 second timeout
        
        print("Connecting...")
        sock.connect((host, port))
        print("✅ Connected successfully!")
        
        # Try to read data
        print("Waiting for data...")
        sock.settimeout(2.0)
        data = sock.recv(1024)
        if data:
            print(f"📨 Received: {data}")
        else:
            print("❌ No data received")
            
        # Try sending something
        print("Sending test message...")
        sock.send(b"GET / HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n")
        
        # Read response
        response = sock.recv(1024)
        if response:
            print(f"📨 Response: {response}")
        else:
            print("❌ No response")
            
    except ConnectionRefusedError:
        print("❌ Connection refused")
    except socket.timeout:
        print("❌ Connection timeout")
    except Exception as e:
        print(f"❌ Error: {e}")
    finally:
        try:
            sock.close()
            print("🔌 Socket closed")
        except:
            pass

if __name__ == "__main__":
    test_tcp_connection()