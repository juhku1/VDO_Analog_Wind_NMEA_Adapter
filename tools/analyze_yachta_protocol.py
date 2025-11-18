#!/usr/bin/env python3
import socket
import time
import threading

def test_raw_tcp():
    """Test raw TCP connection without any HTTP headers"""
    print("=== TEST 1: Raw TCP Connection (ESP32 style) ===")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        
        print("Connecting to 192.168.4.1:6666 (raw TCP)...")
        sock.connect(("192.168.4.1", 6666))
        print("✅ Connected successfully!")
        
        # Just wait for data without sending anything
        print("Waiting for data (no HTTP request sent)...")
        sock.settimeout(3.0)
        data = sock.recv(1024)
        if data:
            print(f"📨 Received: {data.decode('utf-8', errors='ignore')}")
        else:
            print("❌ No data received")
            
    except Exception as e:
        print(f"❌ Error: {e}")
    finally:
        try:
            sock.close()
        except:
            pass
    print()

def test_http_tcp():
    """Test TCP with HTTP request (what worked before)"""
    print("=== TEST 2: TCP with HTTP Request ===")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        
        print("Connecting to 192.168.4.1:6666...")
        sock.connect(("192.168.4.1", 6666))
        print("✅ Connected successfully!")
        
        print("Sending HTTP request...")
        http_request = b"GET / HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n"
        sock.send(http_request)
        
        print("Waiting for response...")
        sock.settimeout(3.0)
        data = sock.recv(1024)
        if data:
            print(f"📨 Received: {data.decode('utf-8', errors='ignore')}")
        else:
            print("❌ No response")
            
    except Exception as e:
        print(f"❌ Error: {e}")
    finally:
        try:
            sock.close()
        except:
            pass
    print()

def test_different_http_methods():
    """Test different HTTP methods to see what triggers NMEA stream"""
    methods = [
        b"GET /nmea HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n",
        b"GET /stream HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n", 
        b"GET /data HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n",
        b"GET / HTTP/1.0\r\nHost: 192.168.4.1\r\n\r\n",
        b"HEAD / HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n",
        b"\r\n"  # Just CRLF
    ]
    
    for i, method in enumerate(methods, 1):
        print(f"=== TEST 3.{i}: Different HTTP Method ===")
        print(f"Request: {method}")
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5.0)
            
            sock.connect(("192.168.4.1", 6666))
            sock.send(method)
            
            sock.settimeout(2.0)
            data = sock.recv(1024)
            if data:
                print(f"📨 Response: {data.decode('utf-8', errors='ignore')[:100]}...")
            else:
                print("❌ No response")
                
        except Exception as e:
            print(f"❌ Error: {e}")
        finally:
            try:
                sock.close()
            except:
                pass
        print()
        time.sleep(1)  # Brief pause between tests

def test_socket_options():
    """Test with different socket options to mimic ESP32"""
    print("=== TEST 4: Socket Options (ESP32-like) ===")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        
        # Set socket options similar to ESP32
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        sock.settimeout(1.0)  # ESP32 uses 1 second timeout
        
        print("Connecting with ESP32-like socket options...")
        sock.connect(("192.168.4.1", 6666))
        print("✅ Connected successfully!")
        
        # Send HTTP request like ESP32 does
        http_request = "GET / HTTP/1.1\r\nHost: 192.168.4.1\r\n\r\n"
        sock.send(http_request.encode())
        print("HTTP request sent (ESP32 style)")
        
        sock.settimeout(3.0)
        data = sock.recv(1024)
        if data:
            print(f"📨 Received: {data.decode('utf-8', errors='ignore')}")
        else:
            print("❌ No response")
            
    except Exception as e:
        print(f"❌ Error: {e}")
    finally:
        try:
            sock.close()
        except:
            pass
    print()

if __name__ == "__main__":
    print("🔍 Analyzing Yachta TCP Protocol Differences")
    print("=" * 50)
    
    # Test 1: Raw TCP (what ESP32 was doing originally)
    test_raw_tcp()
    
    # Test 2: TCP with HTTP (what worked before)  
    test_http_tcp()
    
    # Test 3: Different HTTP methods
    test_different_http_methods()
    
    # Test 4: ESP32-like socket behavior
    test_socket_options()
    
    print("🏁 Tests completed!")