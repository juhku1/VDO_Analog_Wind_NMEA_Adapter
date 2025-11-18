#!/usr/bin/env python3
"""
Simple wind vane simulation to test ESP32 response timing
"""
import socket
import time
import math

# ESP32 configuration
ESP32_IP = "192.168.68.119"
ESP32_PORT = 10110

def send_nmea_direction(sock, angle):
    """Send MWV NMEA sentence with specified wind direction"""
    # Create MWV sentence: $WIMWV,angle,R,speed,N,A*checksum
    sentence_base = f"WIMWV,{angle:.0f},R,5.0,N,A"
    
    # Calculate checksum
    checksum = 0
    for c in sentence_base:
        checksum ^= ord(c)
    
    # Complete NMEA sentence
    sentence = f"${sentence_base}*{checksum:02X}\r\n"
    
    try:
        sock.send(sentence.encode())
        print(f"Sent: {sentence.strip()}")
        return True
    except Exception as e:
        print(f"Send error: {e}")
        return False

def main():
    print(f"Connecting to ESP32 at {ESP32_IP}:{ESP32_PORT}")
    
    try:
        # Create TCP connection
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        sock.connect((ESP32_IP, ESP32_PORT))
        print("✅ Connected to ESP32!")
        
        print("\n🌪️ Starting wind vane simulation...")
        print("This will send rapid direction changes to test VDO response timing")
        
        # Test rapid direction changes
        angles = [0, 45, 90, 135, 180, 225, 270, 315, 0]
        
        for angle in angles:
            if not send_nmea_direction(sock, angle):
                break
            time.sleep(0.2)  # 200ms between changes (5Hz update rate)
        
        print("\n🔄 Testing smooth rotation...")
        # Test smooth rotation
        for i in range(36):  # 360 degrees in 10-degree steps
            angle = i * 10
            if not send_nmea_direction(sock, angle):
                break
            time.sleep(0.1)  # 100ms between changes (10Hz update rate)
            
    except socket.error as e:
        print(f"❌ Connection failed: {e}")
        print("\nTroubleshooting:")
        print("1. Check ESP32 is running and connected to network")
        print("2. Verify ESP32 TCP server is enabled on port 10110")
        print("3. Check firewall settings")
        
    except KeyboardInterrupt:
        print("\n⏹️ Simulation stopped by user")
        
    finally:
        try:
            sock.close()
        except:
            pass
        print("🔌 Connection closed")

if __name__ == "__main__":
    main()