#!/usr/bin/env python3
"""
NMEA Wind & GPS Sender v2
Simplified GUI for testing VDO Wind Adapter

Features:
- Separate TCP and UDP connections
- Select which NMEA sentences to send via each protocol
- Manual controls for wind and GPS data
- No simulation modes - direct control only
"""

import tkinter as tk
from tkinter import ttk, messagebox
import socket
import threading
import time
import math

class NMEASender:
    def __init__(self, root):
        self.root = root
        self.root.title("NMEA Wind & GPS Sender v2")
        self.root.geometry("600x800")
        
        # Connection state
        self.tcp_socket = None
        self.udp_socket = None
        self.tcp_connected = False
        self.udp_bound = False
        self.sending = False
        self.send_thread = None
        
        # Data values
        self.aws_angle = tk.DoubleVar(value=45.0)
        self.aws_speed = tk.DoubleVar(value=12.5)
        self.tws_angle = tk.DoubleVar(value=38.0)
        self.tws_speed = tk.DoubleVar(value=14.2)
        self.sog = tk.DoubleVar(value=6.8)
        self.cog = tk.DoubleVar(value=285.0)
        self.hdg = tk.DoubleVar(value=290.0)
        
        # Connection settings
        self.esp_ip = tk.StringVar(value="192.168.1.50")
        self.tcp_port = tk.IntVar(value=10110)
        self.udp_port = tk.IntVar(value=10110)
        self.tcp_enabled = tk.BooleanVar(value=True)
        self.udp_enabled = tk.BooleanVar(value=True)
        
        # NMEA sentence selection
        self.tcp_mwvr = tk.BooleanVar(value=True)   # Apparent Wind
        self.tcp_mwvt = tk.BooleanVar(value=False)  # True Wind
        self.tcp_vwr = tk.BooleanVar(value=False)   # VWR format
        
        self.udp_rmc = tk.BooleanVar(value=True)    # GPS SOG/COG
        self.udp_hdt = tk.BooleanVar(value=True)    # True Heading
        self.udp_hdm = tk.BooleanVar(value=False)   # Magnetic Heading
        self.udp_vtg = tk.BooleanVar(value=False)   # Track & Speed
        
        # Status
        self.last_tcp_sentence = tk.StringVar(value="--")
        self.last_udp_sentence = tk.StringVar(value="--")
        
        self.create_widgets()
    
    def create_widgets(self):
        # Connection Frame
        conn_frame = ttk.LabelFrame(self.root, text="Connection", padding=10)
        conn_frame.pack(fill="x", padx=10, pady=5)
        
        ttk.Label(conn_frame, text="ESP32 IP:").grid(row=0, column=0, sticky="w")
        ttk.Entry(conn_frame, textvariable=self.esp_ip, width=20).grid(row=0, column=1, padx=5)
        
        ttk.Label(conn_frame, text="TCP Port:").grid(row=1, column=0, sticky="w")
        ttk.Entry(conn_frame, textvariable=self.tcp_port, width=10).grid(row=1, column=1, sticky="w", padx=5)
        ttk.Checkbutton(conn_frame, text="Enable TCP", variable=self.tcp_enabled).grid(row=1, column=2, padx=5)
        
        ttk.Label(conn_frame, text="UDP Port:").grid(row=2, column=0, sticky="w")
        ttk.Entry(conn_frame, textvariable=self.udp_port, width=10).grid(row=2, column=1, sticky="w", padx=5)
        ttk.Checkbutton(conn_frame, text="Enable UDP", variable=self.udp_enabled).grid(row=2, column=2, padx=5)
        
        ttk.Button(conn_frame, text="Connect", command=self.connect).grid(row=3, column=0, columnspan=3, pady=5)
        
        self.status_label = ttk.Label(conn_frame, text="Status: Disconnected", foreground="red")
        self.status_label.grid(row=4, column=0, columnspan=3)
        
        # NMEA Sentences Frame
        nmea_frame = ttk.LabelFrame(self.root, text="NMEA Sentences", padding=10)
        nmea_frame.pack(fill="x", padx=10, pady=5)
        
        ttk.Label(nmea_frame, text="Send via TCP:", font=("Arial", 10, "bold")).grid(row=0, column=0, sticky="w", columnspan=2)
        ttk.Checkbutton(nmea_frame, text="MWV(R) - Apparent Wind", variable=self.tcp_mwvr).grid(row=1, column=0, sticky="w", padx=20)
        ttk.Checkbutton(nmea_frame, text="MWV(T) - True Wind", variable=self.tcp_mwvt).grid(row=2, column=0, sticky="w", padx=20)
        ttk.Checkbutton(nmea_frame, text="VWR - Relative Wind", variable=self.tcp_vwr).grid(row=3, column=0, sticky="w", padx=20)
        
        ttk.Label(nmea_frame, text="Send via UDP:", font=("Arial", 10, "bold")).grid(row=4, column=0, sticky="w", columnspan=2, pady=(10,0))
        ttk.Checkbutton(nmea_frame, text="RMC - GPS (SOG, COG)", variable=self.udp_rmc).grid(row=5, column=0, sticky="w", padx=20)
        ttk.Checkbutton(nmea_frame, text="HDT - True Heading", variable=self.udp_hdt).grid(row=6, column=0, sticky="w", padx=20)
        ttk.Checkbutton(nmea_frame, text="HDM - Magnetic Heading", variable=self.udp_hdm).grid(row=7, column=0, sticky="w", padx=20)
        ttk.Checkbutton(nmea_frame, text="VTG - Track & Speed", variable=self.udp_vtg).grid(row=8, column=0, sticky="w", padx=20)
        
        # Apparent Wind Frame
        aws_frame = ttk.LabelFrame(self.root, text="Apparent Wind (MWV-R)", padding=10)
        aws_frame.pack(fill="x", padx=10, pady=5)
        
        ttk.Label(aws_frame, text="Angle (°):").grid(row=0, column=0, sticky="w")
        ttk.Scale(aws_frame, from_=0, to=359, variable=self.aws_angle, orient="horizontal", length=300).grid(row=0, column=1, padx=5)
        ttk.Label(aws_frame, textvariable=self.aws_angle, width=6).grid(row=0, column=2)
        
        ttk.Label(aws_frame, text="Speed (kn):").grid(row=1, column=0, sticky="w")
        ttk.Scale(aws_frame, from_=0, to=70, variable=self.aws_speed, orient="horizontal", length=300).grid(row=1, column=1, padx=5)
        ttk.Label(aws_frame, textvariable=self.aws_speed, width=6).grid(row=1, column=2)
        
        # True Wind Frame
        tws_frame = ttk.LabelFrame(self.root, text="True Wind (MWV-T)", padding=10)
        tws_frame.pack(fill="x", padx=10, pady=5)
        
        ttk.Label(tws_frame, text="Angle (°):").grid(row=0, column=0, sticky="w")
        ttk.Scale(tws_frame, from_=0, to=359, variable=self.tws_angle, orient="horizontal", length=300).grid(row=0, column=1, padx=5)
        ttk.Label(tws_frame, textvariable=self.tws_angle, width=6).grid(row=0, column=2)
        
        ttk.Label(tws_frame, text="Speed (kn):").grid(row=1, column=0, sticky="w")
        ttk.Scale(tws_frame, from_=0, to=70, variable=self.tws_speed, orient="horizontal", length=300).grid(row=1, column=1, padx=5)
        ttk.Label(tws_frame, textvariable=self.tws_speed, width=6).grid(row=1, column=2)
        
        # GPS Data Frame
        gps_frame = ttk.LabelFrame(self.root, text="GPS Data", padding=10)
        gps_frame.pack(fill="x", padx=10, pady=5)
        
        ttk.Label(gps_frame, text="SOG (kn):").grid(row=0, column=0, sticky="w")
        ttk.Scale(gps_frame, from_=0, to=30, variable=self.sog, orient="horizontal", length=300).grid(row=0, column=1, padx=5)
        ttk.Label(gps_frame, textvariable=self.sog, width=6).grid(row=0, column=2)
        
        ttk.Label(gps_frame, text="COG (°):").grid(row=1, column=0, sticky="w")
        ttk.Scale(gps_frame, from_=0, to=359, variable=self.cog, orient="horizontal", length=300).grid(row=1, column=1, padx=5)
        ttk.Label(gps_frame, textvariable=self.cog, width=6).grid(row=1, column=2)
        
        ttk.Label(gps_frame, text="HDG (°):").grid(row=2, column=0, sticky="w")
        ttk.Scale(gps_frame, from_=0, to=359, variable=self.hdg, orient="horizontal", length=300).grid(row=2, column=1, padx=5)
        ttk.Label(gps_frame, textvariable=self.hdg, width=6).grid(row=2, column=2)
        
        # Control Buttons
        btn_frame = ttk.Frame(self.root, padding=10)
        btn_frame.pack(fill="x", padx=10, pady=5)
        
        self.start_btn = ttk.Button(btn_frame, text="Start Sending", command=self.start_sending)
        self.start_btn.pack(side="left", padx=5)
        
        self.stop_btn = ttk.Button(btn_frame, text="Stop", command=self.stop_sending, state="disabled")
        self.stop_btn.pack(side="left", padx=5)
        
        # Status Frame
        status_frame = ttk.LabelFrame(self.root, text="Last Sent", padding=10)
        status_frame.pack(fill="both", expand=True, padx=10, pady=5)
        
        ttk.Label(status_frame, text="TCP:").grid(row=0, column=0, sticky="w")
        ttk.Label(status_frame, textvariable=self.last_tcp_sentence, font=("Courier", 9)).grid(row=0, column=1, sticky="w", padx=5)
        
        ttk.Label(status_frame, text="UDP:").grid(row=1, column=0, sticky="w")
        ttk.Label(status_frame, textvariable=self.last_udp_sentence, font=("Courier", 9)).grid(row=1, column=1, sticky="w", padx=5)
    
    def nmea_checksum(self, sentence):
        """Calculate NMEA checksum"""
        checksum = 0
        for char in sentence:
            checksum ^= ord(char)
        return f"{checksum:02X}"
    
    def format_nmea(self, sentence):
        """Add $ prefix and *checksum to NMEA sentence"""
        checksum = self.nmea_checksum(sentence)
        return f"${sentence}*{checksum}\r\n"
    
    def generate_mwv_r(self):
        """Generate MWV Apparent Wind sentence"""
        angle = self.aws_angle.get()
        speed = self.aws_speed.get()
        side = "R" if angle <= 180 else "L"
        if angle > 180:
            angle = 360 - angle
        sentence = f"IIMWV,{angle:.1f},{side},{speed:.1f},N,A"
        return self.format_nmea(sentence)
    
    def generate_mwv_t(self):
        """Generate MWV True Wind sentence"""
        angle = self.tws_angle.get()
        speed = self.tws_speed.get()
        side = "R" if angle <= 180 else "L"
        if angle > 180:
            angle = 360 - angle
        sentence = f"IIMWV,{angle:.1f},{side},{speed:.1f},N,A"
        return self.format_nmea(sentence)
    
    def generate_vwr(self):
        """Generate VWR Relative Wind sentence"""
        angle = self.aws_angle.get()
        speed_kn = self.aws_speed.get()
        speed_ms = speed_kn * 0.514444
        speed_kmh = speed_kn * 1.852
        side = "R" if angle <= 180 else "L"
        if angle > 180:
            angle = 360 - angle
        sentence = f"IIVWR,{angle:.1f},{side},{speed_kn:.1f},N,{speed_ms:.1f},M,{speed_kmh:.1f},K"
        return self.format_nmea(sentence)
    
    def generate_rmc(self):
        """Generate RMC GPS sentence"""
        sog = self.sog.get()
        cog = self.cog.get()
        # Simplified RMC with minimal fields
        sentence = f"GPRMC,120000,A,6000.00,N,02500.00,E,{sog:.1f},{cog:.1f},231123,,,A"
        return self.format_nmea(sentence)
    
    def generate_hdt(self):
        """Generate HDT True Heading sentence"""
        hdg = self.hdg.get()
        sentence = f"GPHDT,{hdg:.1f},T"
        return self.format_nmea(sentence)
    
    def generate_hdm(self):
        """Generate HDM Magnetic Heading sentence"""
        hdg = self.hdg.get()
        sentence = f"GPHDM,{hdg:.1f},M"
        return self.format_nmea(sentence)
    
    def generate_vtg(self):
        """Generate VTG Track & Speed sentence"""
        sog = self.sog.get()
        cog = self.cog.get()
        sog_kmh = sog * 1.852
        sentence = f"GPVTG,{cog:.1f},T,,M,{sog:.1f},N,{sog_kmh:.1f},K,A"
        return self.format_nmea(sentence)
    
    def connect(self):
        """Establish TCP and/or UDP connections"""
        try:
            ip = self.esp_ip.get()
            
            # TCP Connection
            if self.tcp_enabled.get():
                if self.tcp_socket:
                    self.tcp_socket.close()
                self.tcp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                self.tcp_socket.settimeout(5)
                self.tcp_socket.connect((ip, self.tcp_port.get()))
                self.tcp_connected = True
            
            # UDP Connection
            if self.udp_enabled.get():
                if self.udp_socket:
                    self.udp_socket.close()
                self.udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                self.udp_bound = True
            
            # Update status
            status_parts = []
            if self.tcp_connected:
                status_parts.append("TCP ✓")
            if self.udp_bound:
                status_parts.append("UDP ✓")
            
            if status_parts:
                self.status_label.config(text=f"Status: {' '.join(status_parts)}", foreground="green")
                messagebox.showinfo("Success", "Connected successfully!")
            else:
                self.status_label.config(text="Status: No protocols enabled", foreground="orange")
                
        except Exception as e:
            messagebox.showerror("Connection Error", f"Failed to connect:\n{e}")
            self.status_label.config(text="Status: Connection failed", foreground="red")
    
    def send_loop(self):
        """Main sending loop"""
        while self.sending:
            try:
                # Send TCP sentences
                if self.tcp_connected and self.tcp_socket:
                    tcp_sentences = []
                    if self.tcp_mwvr.get():
                        sentence = self.generate_mwv_r()
                        tcp_sentences.append(sentence)
                        self.tcp_socket.send(sentence.encode())
                    if self.tcp_mwvt.get():
                        sentence = self.generate_mwv_t()
                        tcp_sentences.append(sentence)
                        self.tcp_socket.send(sentence.encode())
                    if self.tcp_vwr.get():
                        sentence = self.generate_vwr()
                        tcp_sentences.append(sentence)
                        self.tcp_socket.send(sentence.encode())
                    
                    if tcp_sentences:
                        self.last_tcp_sentence.set(tcp_sentences[-1].strip())
                
                # Send UDP sentences
                if self.udp_bound and self.udp_socket:
                    udp_sentences = []
                    ip = self.esp_ip.get()
                    port = self.udp_port.get()
                    
                    if self.udp_rmc.get():
                        sentence = self.generate_rmc()
                        udp_sentences.append(sentence)
                        self.udp_socket.sendto(sentence.encode(), (ip, port))
                    if self.udp_hdt.get():
                        sentence = self.generate_hdt()
                        udp_sentences.append(sentence)
                        self.udp_socket.sendto(sentence.encode(), (ip, port))
                    if self.udp_hdm.get():
                        sentence = self.generate_hdm()
                        udp_sentences.append(sentence)
                        self.udp_socket.sendto(sentence.encode(), (ip, port))
                    if self.udp_vtg.get():
                        sentence = self.generate_vtg()
                        udp_sentences.append(sentence)
                        self.udp_socket.sendto(sentence.encode(), (ip, port))
                    
                    if udp_sentences:
                        self.last_udp_sentence.set(udp_sentences[-1].strip())
                
                time.sleep(1)  # Send every 1 second
                
            except Exception as e:
                print(f"Send error: {e}")
                self.sending = False
                self.root.after(0, self.stop_sending)
    
    def start_sending(self):
        """Start sending NMEA data"""
        if not self.tcp_connected and not self.udp_bound:
            messagebox.showwarning("Not Connected", "Please connect first!")
            return
        
        self.sending = True
        self.start_btn.config(state="disabled")
        self.stop_btn.config(state="normal")
        
        self.send_thread = threading.Thread(target=self.send_loop, daemon=True)
        self.send_thread.start()
    
    def stop_sending(self):
        """Stop sending NMEA data"""
        self.sending = False
        self.start_btn.config(state="normal")
        self.stop_btn.config(state="disabled")
    
    def cleanup(self):
        """Cleanup on exit"""
        self.sending = False
        if self.tcp_socket:
            try:
                self.tcp_socket.close()
            except:
                pass
        if self.udp_socket:
            try:
                self.udp_socket.close()
            except:
                pass

def main():
    root = tk.Tk()
    app = NMEASender(root)
    root.protocol("WM_DELETE_WINDOW", lambda: (app.cleanup(), root.destroy()))
    root.mainloop()

if __name__ == "__main__":
    main()
