#!/usr/bin/env python3
import socket
import time

def test_multiple_connections():
    """Test what happens with multiple simultaneous connections"""
    print("=== Multiple Connection Test (Critical for ESP32) ===")
    
    connections = []
    try:
        # Try to open multiple connections quickly
        for i in range(5):
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5.0)
            print(f"Opening connection {i+1}...")
            
            try:
                start = time.time()
                sock.connect(("192.168.4.1", 6666))
                connect_time = time.time() - start
                connections.append(sock)
                print(f"  ✅ Connection {i+1} successful in {connect_time:.3f}s")
                
                # Try to read data immediately
                sock.settimeout(0.5)
                try:
                    data = sock.recv(1024)
                    if data:
                        nmea = data.decode('utf-8', errors='ignore').strip()
                        print(f"  📨 Data on conn {i+1}: {nmea}")
                except socket.timeout:
                    print(f"  ⏱️ No immediate data on conn {i+1}")
                    
            except Exception as e:
                print(f"  ❌ Connection {i+1} failed: {e}")
                if "refused" in str(e).lower():
                    print(f"    📝 Connection refused - likely max clients reached")
                elif "reset" in str(e).lower():
                    print(f"    📝 Connection reset - server rejected")
                
        print(f"\n📊 Result: {len(connections)} successful connections out of 5 attempts")
        
        if len(connections) > 1:
            print("📍 Multiple connections allowed - not a single-client limitation")
        elif len(connections) == 1:
            print("⚠️ Only one connection allowed - single-client server!")
        else:
            print("❌ No connections successful")
        
    finally:
        # Close all connections and test reconnection
        for i, sock in enumerate(connections):
            try:
                sock.close()
                print(f"Closed connection {i+1}")
            except:
                pass
                
        # Test immediate reconnection after closing all
        print("\n🔄 Testing immediate reconnection...")
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(2.0)
            start = time.time()
            sock.connect(("192.168.4.1", 6666))
            reconnect_time = time.time() - start
            print(f"  ✅ Reconnected in {reconnect_time:.3f}s after closing all")
            sock.close()
        except Exception as e:
            print(f"  ❌ Reconnection failed: {e}")

def test_esp32_simulation():
    """Simulate ESP32 connection pattern"""
    print("\n=== ESP32 Connection Pattern Simulation ===")
    
    # ESP32 tries every 3 seconds
    for attempt in range(3):
        print(f"\nAttempt {attempt + 1} (ESP32 pattern):")
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            
            # ESP32 socket options
            sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            sock.settimeout(1.0)  # ESP32 uses 1 second timeout
            
            start = time.time()
            sock.connect(("192.168.4.1", 6666))
            connect_time = time.time() - start
            print(f"  ✅ Connected in {connect_time:.3f}s")
            
            # Read data like ESP32
            data = sock.recv(1472)  # ESP32 netBuf size
            if data:
                print(f"  📨 Received: {data.decode('utf-8', errors='ignore').strip()}")
            
            sock.close()
            
        except Exception as e:
            print(f"  ❌ Failed (ESP32 style): {e}")
            if "reset" in str(e).lower():
                print("    📝 This is the same error ESP32 gets!")
        
        if attempt < 2:
            print("  ⏱️ Waiting 3 seconds (ESP32 retry interval)...")
            time.sleep(3)

def test_connection_load():
    """Test rapid connections to see if rate limiting exists"""
    print("\n=== Connection Rate Test ===")
    
    success_count = 0
    for i in range(10):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(1.0)
            
            start = time.time()
            sock.connect(("192.168.4.1", 6666))
            connect_time = time.time() - start
            success_count += 1
            
            print(f"  Connection {i+1}: OK ({connect_time:.3f}s)")
            sock.close()
            
            # Small delay between connections
            time.sleep(0.1)
            
        except Exception as e:
            print(f"  Connection {i+1}: FAILED - {e}")
    
    print(f"\n📊 Success rate: {success_count}/10")
    if success_count < 10:
        print("⚠️ Some connections failed - possible rate limiting or resource limits")

if __name__ == "__main__":
    print("🔬 ESP32 Connection Analysis")
    print("=" * 40)
    
    test_multiple_connections()
    test_esp32_simulation()
    test_connection_load()
    
    print("\n🏁 Analysis complete!")