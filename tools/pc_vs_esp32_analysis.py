#!/usr/bin/env python3
import socket
import time
import sys

def test_connection_details():
    """Test connection with detailed socket information"""
    print("=== Detailed Connection Analysis ===")
    
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        
        # Get local socket info before connection
        sock.bind(('', 0))  # Bind to any available port
        local_port = sock.getsockname()[1]
        print(f"PC Local port: {local_port}")
        
        # Set socket options (see what defaults are)
        print("Socket options before connection:")
        try:
            print(f"  SO_RCVBUF: {sock.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)}")
            print(f"  SO_SNDBUF: {sock.getsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF)}")
            print(f"  TCP_NODELAY: {sock.getsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY)}")
        except:
            pass
        
        print(f"Connecting to 192.168.4.1:6666...")
        sock.connect(("192.168.4.1", 6666))
        
        # Get connection details
        local_addr = sock.getsockname()
        remote_addr = sock.getpeername()
        print(f"✅ Connected: {local_addr} -> {remote_addr}")
        
        # Try to get more socket info after connection
        print("Socket options after connection:")
        try:
            print(f"  SO_RCVBUF: {sock.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)}")
            print(f"  SO_SNDBUF: {sock.getsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF)}")
            print(f"  TCP_NODELAY: {sock.getsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY)}")
        except:
            pass
        
        # Wait for data and measure timing
        start_time = time.time()
        sock.settimeout(2.0)
        data = sock.recv(1024)
        receive_time = time.time() - start_time
        
        if data:
            print(f"📨 Data received after {receive_time:.3f}s: {data.decode('utf-8', errors='ignore').strip()}")
        
        sock.close()
        
    except Exception as e:
        print(f"❌ Error: {e}")

def test_multiple_quick_connections():
    """Test rapid connections to see behavior pattern"""
    print("\n=== Rapid Connection Test ===")
    
    for i in range(3):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5.0)
            
            start_time = time.time()
            sock.connect(("192.168.4.1", 6666))
            connect_time = time.time() - start_time
            
            local_port = sock.getsockname()[1]
            print(f"Connection {i+1}: SUCCESS in {connect_time:.3f}s (local port {local_port})")
            
            # Try to read data immediately
            sock.settimeout(1.0)
            try:
                data = sock.recv(1024)
                if data:
                    print(f"  📨 Data: {data.decode('utf-8', errors='ignore').strip()}")
                else:
                    print(f"  📭 No data")
            except socket.timeout:
                print(f"  ⏱️ Timeout waiting for data")
            
            sock.close()
            time.sleep(0.5)  # Brief pause
            
        except Exception as e:
            print(f"Connection {i+1}: FAILED - {e}")

def test_different_source_ports():
    """Test from different source ports to see if that matters"""
    print("\n=== Source Port Test ===")
    
    test_ports = [0, 12345, 23456, 34567]  # 0 means system chooses
    
    for port in test_ports:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(3.0)
            
            if port != 0:
                sock.bind(('', port))
            
            actual_port = sock.getsockname()[1]
            print(f"Testing from local port {actual_port}...")
            
            sock.connect(("192.168.4.1", 6666))
            print(f"  ✅ Connected from port {actual_port}")
            
            sock.settimeout(1.0)
            data = sock.recv(1024)
            if data:
                print(f"  📨 Data received")
            
            sock.close()
            
        except Exception as e:
            print(f"  ❌ Failed from port {port}: {e}")

def test_connection_timing():
    """Test connection timing patterns"""
    print("\n=== Connection Timing Pattern Test ===")
    
    # Test immediate reconnection
    for attempt in range(3):
        print(f"\nAttempt {attempt + 1}:")
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(2.0)
            
            start = time.time()
            sock.connect(("192.168.4.1", 6666))
            connect_time = time.time() - start
            
            print(f"  ✅ Connected in {connect_time:.3f}s")
            sock.close()
            
            # NO delay between connections (like ESP32 retry pattern)
            
        except Exception as e:
            print(f"  ❌ Failed: {e}")

if __name__ == "__main__":
    print("🔬 PC vs ESP32 Connection Comparison")
    print("=" * 50)
    
    test_connection_details()
    test_multiple_quick_connections()
    test_different_source_ports()
    test_connection_timing()
    
    print("\n💡 This data will help identify what ESP32 might be doing differently")
    print("🏁 Analysis complete!")