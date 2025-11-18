#!/usr/bin/env python3
import socket
import time
import struct

def test_connection_timing():
    """Test connection timing and behavior"""
    print("=== Connection Timing Analysis ===")
    
    for i in range(5):
        try:
            start_time = time.time()
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(10.0)
            
            print(f"Attempt {i+1}: Connecting...")
            sock.connect(("192.168.4.1", 6666))
            connect_time = time.time() - start_time
            print(f"  ✅ Connected in {connect_time:.3f} seconds")
            
            # Wait for immediate data
            sock.settimeout(1.0)
            data = sock.recv(1024)
            if data:
                print(f"  📨 Immediate data: {data.decode('utf-8', errors='ignore').strip()}")
            
            sock.close()
            time.sleep(2)  # Wait between connections
            
        except Exception as e:
            print(f"  ❌ Failed: {e}")
    print()

def test_multiple_connections():
    """Test what happens with multiple simultaneous connections"""
    print("=== Multiple Connection Test ===")
    
    connections = []
    try:
        # Try to open multiple connections
        for i in range(3):
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5.0)
            print(f"Opening connection {i+1}...")
            
            try:
                sock.connect(("192.168.4.1", 6666))
                connections.append(sock)
                print(f"  ✅ Connection {i+1} successful")
                
                # Try to read data
                sock.settimeout(1.0)
                data = sock.recv(1024)
                if data:
                    print(f"  📨 Data on connection {i+1}: {data.decode('utf-8', errors='ignore').strip()}")
                    
            except Exception as e:
                print(f"  ❌ Connection {i+1} failed: {e}")
                
        print(f"\nTotal successful connections: {len(connections)}")
        
    finally:
        # Close all connections
        for i, sock in enumerate(connections):
            try:
                sock.close()
                print(f"Closed connection {i+1}")
            except:
                pass
    print()

def test_socket_buffer_sizes():
    """Test different socket buffer sizes"""
    print("=== Socket Buffer Size Test ===")
    
    buffer_sizes = [1024, 2048, 4096, 8192]
    
    for buf_size in buffer_sizes:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            
            # Set buffer sizes
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, buf_size)
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, buf_size)
            sock.settimeout(5.0)
            
            print(f"Testing with buffer size {buf_size}...")
            sock.connect(("192.168.4.1", 6666))
            
            # Check actual buffer sizes
            actual_rcv = sock.getsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF)
            actual_snd = sock.getsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF)
            print(f"  Actual buffers: RCV={actual_rcv}, SND={actual_snd}")
            
            sock.settimeout(1.0)
            data = sock.recv(1024)
            if data:
                print(f"  📨 Data received: {len(data)} bytes")
            
            sock.close()
            
        except Exception as e:
            print(f"  ❌ Failed with buffer {buf_size}: {e}")
    print()

def test_connection_persistence():
    """Test how long connections stay alive"""
    print("=== Connection Persistence Test ===")
    
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        
        start_time = time.time()
        sock.connect(("192.168.4.1", 6666))
        print("✅ Connected, monitoring for disconnection...")
        
        message_count = 0
        while True:
            try:
                sock.settimeout(1.0)
                data = sock.recv(1024)
                if data:
                    message_count += 1
                    elapsed = time.time() - start_time
                    print(f"  Message {message_count} at {elapsed:.1f}s: {data.decode('utf-8', errors='ignore').strip()}")
                else:
                    print("  📤 Connection closed by server")
                    break
            except socket.timeout:
                # Check if still connected
                try:
                    sock.settimeout(0.1)
                    data = sock.recv(1024, socket.MSG_DONTWAIT)
                except:
                    elapsed = time.time() - start_time
                    if elapsed > 30:  # Stop after 30 seconds
                        print(f"  ⏱️ Test stopped after {elapsed:.1f} seconds")
                        break
                    continue
            except Exception as e:
                elapsed = time.time() - start_time
                print(f"  ❌ Connection lost after {elapsed:.1f}s: {e}")
                break
                
        sock.close()
        
    except Exception as e:
        print(f"❌ Failed: {e}")
    print()

def test_tcp_options():
    """Test different TCP options"""
    print("=== TCP Options Analysis ===")
    
    test_configs = [
        {"name": "Default", "options": {}},
        {"name": "No Delay", "options": {socket.IPPROTO_TCP: {socket.TCP_NODELAY: 1}}},
        {"name": "Keep Alive", "options": {socket.SOL_SOCKET: {socket.SO_KEEPALIVE: 1}}},
        {"name": "Reuse Addr", "options": {socket.SOL_SOCKET: {socket.SO_REUSEADDR: 1}}},
        {"name": "All Options", "options": {
            socket.SOL_SOCKET: {socket.SO_KEEPALIVE: 1, socket.SO_REUSEADDR: 1},
            socket.IPPROTO_TCP: {socket.TCP_NODELAY: 1}
        }}
    ]
    
    for config in test_configs:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            
            # Apply socket options
            for level, opts in config["options"].items():
                for option, value in opts.items():
                    sock.setsockopt(level, option, value)
            
            sock.settimeout(5.0)
            
            print(f"Testing {config['name']} configuration...")
            start_time = time.time()
            sock.connect(("192.168.4.1", 6666))
            connect_time = time.time() - start_time
            
            print(f"  ✅ Connected in {connect_time:.3f}s")
            
            sock.settimeout(1.0)
            data = sock.recv(1024)
            if data:
                print(f"  📨 Data: {data.decode('utf-8', errors='ignore').strip()}")
            
            sock.close()
            
        except Exception as e:
            print(f"  ❌ Failed: {e}")
    print()

def analyze_network_interface():
    """Get information about network interface"""
    print("=== Network Interface Analysis ===")
    try:
        import subprocess
        
        # Get IP configuration
        result = subprocess.run(['ipconfig'], capture_output=True, text=True)
        lines = result.stdout.split('\n')
        
        in_wireless = False
        for line in lines:
            if 'Wireless LAN adapter WLAN:' in line:
                in_wireless = True
                print("📡 Wireless interface details:")
            elif in_wireless:
                if line.strip():
                    if 'IPv4 Address' in line:
                        ip = line.split(':')[1].strip()
                        print(f"  Local IP: {ip}")
                    elif 'Default Gateway' in line and '192.168.4' in line:
                        gw = line.split(':')[1].strip()
                        print(f"  Gateway: {gw}")
                elif not line.strip():
                    in_wireless = False
                    
        # Test connectivity
        print("\n🔗 Connectivity test:")
        result = subprocess.run(['ping', '-n', '1', '192.168.4.1'], 
                              capture_output=True, text=True)
        if 'Reply from' in result.stdout:
            # Extract RTT
            for line in result.stdout.split('\n'):
                if 'time=' in line:
                    rtt = line.split('time=')[1].split('ms')[0]
                    print(f"  Ping RTT: {rtt}ms")
                    break
        else:
            print("  ❌ Ping failed")
            
    except Exception as e:
        print(f"❌ Network analysis failed: {e}")
    print()

if __name__ == "__main__":
    print("🔬 Deep TCP Protocol Analysis")
    print("=" * 50)
    
    # Analyze network setup
    analyze_network_interface()
    
    # Test connection timing
    test_connection_timing()
    
    # Test multiple connections
    test_multiple_connections()
    
    # Test socket options
    test_tcp_options()
    
    # Test buffer sizes
    test_socket_buffer_sizes()
    
    # Test connection persistence
    test_connection_persistence()
    
    print("🏁 Deep analysis completed!")