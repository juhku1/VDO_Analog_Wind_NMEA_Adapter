import tkinter as tk
import math
import socket
import random
from tkinter import messagebox

# ============================ ASETUKSET ============================
# Oletusasetukset - voi muuttaa Settings-ikkunasta
ESP_IP = "192.168.68.124"  # Kohde ESP32:n osoite
ESP_PORT = 10110           # Kohde portti
LOCAL_IP = "192.168.68.126"  # Oma IP (binding varten)

RADIUS = 294
MARGIN = 30
ARROW_WIDTH = 7
CIRCLE_WIDTH = 3
TICK_STEP = 30

FONT_MAIN = ("Arial", 18, "bold")
FONT_WS   = ("Arial", 18, "bold")
FONT_DBG  = ("Consolas", 13, "bold")
FONT_LBL  = ("Arial", 15, "bold")

SPEED_MIN = 0.0
SPEED_MAX = 70.0
SPEED_INIT = 10.5
SPEED_STEP_FINE = 0.1
SPEED_STEP_COARSE = 1.0

DEFAULT_SENTENCES = ("VWR",)
AWA_INIT = 45.0
# ===================================================================

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
# Bindaa omaan IP:hen jotta ESP32 ei hylkää paketteja
def recreate_socket():
    global sock
    try:
        if sock:
            sock.close()
    except:
        pass
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.bind((LOCAL_IP, 0))
        print(f"Socket bound to {LOCAL_IP}")
    except Exception as e:
        print(f"Socket bind failed: {e}, using default")

recreate_socket()

center = (RADIUS + MARGIN, RADIUS + MARGIN)
awa_deg = AWA_INIT
speed_kn = SPEED_INIT
sentence_types = ["VWR", "MWV(R)", "MWV(T)", "VWT"]  # Lausevaihtoehdot
selected_sentences = None  # Luodaan myöhemmin root:in jälkeen

# Demotila
demo_on = None  # Luodaan myöhemmin
_demo_side_right = True
_demo_base_angle = 45.0
_demo_tick_count = 0

# GPS Simulator
gps_sim_on = None  # Luodaan myöhemmin
_gps_lat = 60.1699  # Helsinki
_gps_lon = 24.9384  # Helsinki
_gps_heading = 135.0  # SE
_gps_speed = 5.0     # 3-8 solmua
_gps_tick_count = 0

# ============================ NMEA ================================
def calculate_checksum(nmea_payload: str) -> str:
    cs = 0
    for c in nmea_payload:
        cs ^= ord(c)
    return f"{cs:02X}"

def to_vwr_components(angle_deg: float):
    a = angle_deg % 360
    if a <= 180:
        return a, 'R'
    else:
        return 360 - a, 'L'

def build_sentence(angle_deg: float, speed_kn: float, stype: str) -> str:
    """Rakenna VWR/VWT/MWV-lauseet oikeilla kentillä ja yksiköillä."""
    a360 = angle_deg % 360
    # Nopea muunnos eri yksiköihin
    spd_kn = float(speed_kn)
    spd_ms = spd_kn * 0.514444
    spd_kmh = spd_kn * 1.852

    if stype == "VWR":
        mag, side = to_vwr_components(a360)
        # $WIVWR,<angle>,<L/R>,<kn>,N,<m/s>,M,<km/h>,K
        payload = f"WIVWR,{mag:.1f},{side},{spd_kn:.1f},N,{spd_ms:.2f},M,{spd_kmh:.2f},K"
    elif stype == "VWT":
        mag, side = to_vwr_components(a360)  # formaatti sama kuin VWR
        payload = f"WIVWT,{mag:.1f},{side},{spd_kn:.1f},N,{spd_ms:.2f},M,{spd_kmh:.2f},K"
    elif stype == "MWV(R)":
        # Apparent (Relative)
        payload = f"WIMWV,{a360:.1f},R,{spd_kn:.1f},N,A"
    elif stype == "MWV(T)":
        # True
        payload = f"WIMWV,{a360:.1f},T,{spd_kn:.1f},N,A"
    else:
        # Oletuksena VWR
        mag, side = to_vwr_components(a360)
        payload = f"WIVWR,{mag:.1f},{side},{spd_kn:.1f},N,{spd_ms:.2f},M,{spd_kmh:.2f},K"

    return f"${payload}*{calculate_checksum(payload)}\r\n"

def build_gps_sentence(lat, lon, speed_kn, heading) -> str:
    """Generoi GPS NMEA RMC -lauseen."""
    import time

    # Desimaaliasteet -> asteet,minuutit
    lat_deg = int(abs(lat))
    lat_min = (abs(lat) - lat_deg) * 60
    lat_dir = 'N' if lat >= 0 else 'S'

    lon_deg = int(abs(lon))
    lon_min = (abs(lon) - lon_deg) * 60
    lon_dir = 'E' if lon >= 0 else 'W'

    # UTC aika ja päivä
    utc_time = time.strftime("%H%M%S", time.gmtime())
    utc_date = time.strftime("%d%m%y", time.gmtime())

    # $GPRMC,time,status,lat,lat_dir,lon,lon_dir,speed,heading,date,mag_var,mag_dir
    payload = (
        f"GPRMC,{utc_time},A,"
        f"{lat_deg:02d}{lat_min:07.4f},{lat_dir},"
        f"{lon_deg:03d}{lon_min:07.4f},{lon_dir},"
        f"{speed_kn:.1f},{heading:.1f},{utc_date},,,"
    )
    return f"${payload}*{calculate_checksum(payload)}\r\n"

def send_nmea(angle_deg, speed_kn):
    lines = []
    for stype, var in selected_sentences.items():
        if var.get():
            line = build_sentence(angle_deg, speed_kn, stype)
            sock.sendto(line.encode("ascii"), (ESP_IP, ESP_PORT))
            lines.append(line.strip())

    # GPS simulator
    if gps_sim_on and gps_sim_on.get():
        gps_line = build_gps_sentence(_gps_lat, _gps_lon, _gps_speed, _gps_heading)
        sock.sendto(gps_line.encode("ascii"), (ESP_IP, ESP_PORT))
        lines.append(f"GPS: {gps_line.strip()}")

    return lines
# ===================================================================

def draw_ticks():
    for ang in range(0, 360, TICK_STEP):
        theta = math.radians(ang)
        x_outer = center[0] + RADIUS * math.sin(theta)
        y_outer = center[1] - RADIUS * math.cos(theta)
        x_inner = center[0] + (RADIUS - 15) * math.sin(theta)
        y_inner = center[1] - (RADIUS - 15) * math.cos(theta)
        canvas.create_line(x_inner, y_inner, x_outer, y_outer,
                           fill="gray", width=1, tags="ticks")

def arrow_color(side: str) -> str:
    return "green" if side == "R" else "red"

def update_arrow(angle):
    global awa_deg
    awa_deg = angle % 360
    canvas.delete("arrow")
    theta = math.radians(awa_deg)
    x = center[0] + RADIUS * math.sin(theta)
    y = center[1] - RADIUS * math.cos(theta)

    mag_vwr, side_vwr = to_vwr_components(awa_deg)
    color = arrow_color(side_vwr)

    canvas.create_line(center[0], center[1], x, y,
                       arrow=tk.LAST, fill=color,
                       width=ARROW_WIDTH, tags="arrow")

    nmea_lines = send_nmea(awa_deg, speed_kn)

    # Näytä kaikki lähetetyt lauseet rivitettyinä
    if nmea_lines:
        sent_text = "Sent:\n" + "\n".join(nmea_lines)
    else:
        sent_text = "Sent: (no sentences selected)"
    dbg_sent.config(text=sent_text)
    dbg_raw.config(text=f"AWA raw: {awa_deg:.1f}°")

    # Näytä ensimmäisen lauseen tiedot pääreadoutissa
    for stype in sentence_types:
        if selected_sentences[stype].get():
            if stype in ("VWR", "VWT"):
                readout_main.config(text=f"{stype}: {mag_vwr:.1f}° {side_vwr}")
            elif stype == "MWV(R)":
                readout_main.config(text=f"MWV(R): {awa_deg:.1f}° R")
            elif stype == "MWV(T)":
                readout_main.config(text=f"MWV(T): {awa_deg:.1f}° T")
            else:
                readout_main.config(text=f"{stype}: {mag_vwr:.1f}° {side_vwr}")
            break
    readout_ws.config(text=f"WS: {speed_kn:.1f} kn")

def mouse_move(event):
    dx = event.x - center[0]
    dy = event.y - center[1]
    angle = (math.degrees(math.atan2(dx, -dy)) % 360)
    update_arrow(angle)

def speed_changed(val):
    global speed_kn
    try:
        speed_kn = float(val)
    except ValueError:
        return
    update_arrow(awa_deg)

def nudge_speed(delta):
    new_val = min(SPEED_MAX, max(SPEED_MIN, speed_kn + delta))
    speed_scale.set(round(new_val, 1))

# ============================ DEMOTILA ============================
def on_demo_toggle():
    if demo_on.get():
        demo_tick()

def demo_tick():
    global awa_deg, speed_kn, _demo_tick_count

    if not demo_on.get():
        return

    # Joka 20 s: 80° käännös, satunnainen suunta
    if _demo_tick_count % 100 == 0:
        direction80 = random.choice([-1, 1])
        demo_tick.target = (awa_deg + direction80 * 80) % 360
        demo_tick.start = awa_deg
        demo_tick.steps = 15  # ~3 s nopea siirtymä
        demo_tick.step = 0
    # Joka 5 s: 20° käännös (jos ei 80° menossa)
    elif _demo_tick_count % 25 == 0:
        direction20 = random.choice([-1, 1])
        demo_tick.target = (awa_deg + direction20 * 20) % 360
        demo_tick.start = awa_deg
        demo_tick.steps = 25  # ~5 s pehmeä siirtymä
        demo_tick.step = 0

    # Interpoloi, jos siirtymä kesken
    if hasattr(demo_tick, 'target') and demo_tick.step < demo_tick.steps:
        frac = demo_tick.step / demo_tick.steps
        delta = (demo_tick.target - demo_tick.start + 540) % 360 - 180
        angle = (demo_tick.start + frac * delta) % 360
        demo_tick.step += 1
    else:
        angle = awa_deg

    speed_kn = max(SPEED_MIN, min(SPEED_MAX, speed_kn + random.uniform(-0.2, 0.2)))
    update_arrow(angle)

    _demo_tick_count += 1
    root.after(200, demo_tick)

def gps_tick():
    """Päivittää GPS-position ja nopeuden."""
    global _gps_lat, _gps_lon, _gps_speed, _gps_heading, _gps_tick_count

    if not gps_sim_on or not gps_sim_on.get():
        return

    # Satunnainen nopeus 3–8 kn, vaihda 5 s välein
    if _gps_tick_count % 25 == 0:
        _gps_speed = random.uniform(3.0, 8.0)

    # Liike m/s
    speed_ms = _gps_speed * 0.514444
    distance_per_tick = speed_ms * 0.2  # 200 ms tick

    # Muunna metrit asteiksi (karkeasti)
    lat_delta = -(distance_per_tick / 111320.0) * math.cos(math.radians(_gps_heading))
    lon_delta = (distance_per_tick / 111320.0) * math.sin(math.radians(_gps_heading)) / math.cos(math.radians(_gps_lat))

    _gps_lat += lat_delta
    _gps_lon += lon_delta

    # Pieni kurssivaihtelu 10 s välein
    if _gps_tick_count % 50 == 0:
        _gps_heading = (_gps_heading + random.uniform(-5, 5)) % 360

    _gps_tick_count += 1
    root.after(200, gps_tick)

def on_gps_toggle():
    if gps_sim_on.get():
        gps_tick()

# Settings window
def open_settings():
    def save_settings():
        global ESP_IP, ESP_PORT, LOCAL_IP
        try:
            new_esp_ip = esp_ip_entry.get().strip()
            new_esp_port = int(esp_port_entry.get().strip())
            new_local_ip = local_ip_entry.get().strip()
            
            # Validate IP addresses
            socket.inet_aton(new_esp_ip)
            if new_local_ip:
                socket.inet_aton(new_local_ip)
            
            # Update globals
            ESP_IP = new_esp_ip
            ESP_PORT = new_esp_port
            LOCAL_IP = new_local_ip
            
            # Recreate socket with new settings
            recreate_socket()
            
            settings_win.destroy()
            messagebox.showinfo("Settings", f"Network settings updated:\nTarget: {ESP_IP}:{ESP_PORT}\nLocal: {LOCAL_IP}")
            
        except ValueError as e:
            messagebox.showerror("Error", f"Invalid settings: {e}")
        except Exception as e:
            messagebox.showerror("Error", f"Settings error: {e}")
    
    settings_win = tk.Toplevel()
    settings_win.title("Network Settings")
    settings_win.geometry("400x200")
    settings_win.resizable(False, False)
    
    frame = tk.Frame(settings_win, padx=20, pady=20)
    frame.pack(fill="both", expand=True)
    
    # Target ESP32 IP
    tk.Label(frame, text="Target ESP32 IP:").grid(row=0, column=0, sticky="w", pady=5)
    esp_ip_entry = tk.Entry(frame, width=20)
    esp_ip_entry.grid(row=0, column=1, padx=10, pady=5)
    esp_ip_entry.insert(0, ESP_IP)
    
    # Target port
    tk.Label(frame, text="Target Port:").grid(row=1, column=0, sticky="w", pady=5)
    esp_port_entry = tk.Entry(frame, width=20)
    esp_port_entry.grid(row=1, column=1, padx=10, pady=5)
    esp_port_entry.insert(0, str(ESP_PORT))
    
    # Local IP (for binding)
    tk.Label(frame, text="Local IP (optional):").grid(row=2, column=0, sticky="w", pady=5)
    local_ip_entry = tk.Entry(frame, width=20)
    local_ip_entry.grid(row=2, column=1, padx=10, pady=5)
    local_ip_entry.insert(0, LOCAL_IP)
    
    # Info
    info_text = ("Local IP: Your computer's IP address.\n"
                "Leave empty for automatic selection.\n"
                "Use 'ipconfig' (Windows) or 'ifconfig' (Linux/Mac) to find it.")
    tk.Label(frame, text=info_text, font=("Arial", 9), fg="gray", justify="left").grid(row=3, column=0, columnspan=2, pady=10)
    
    # Buttons
    btn_frame = tk.Frame(frame)
    btn_frame.grid(row=4, column=0, columnspan=2, pady=10)
    tk.Button(btn_frame, text="Save", command=save_settings).pack(side="left", padx=5)
    tk.Button(btn_frame, text="Cancel", command=settings_win.destroy).pack(side="left", padx=5)

# ============================== UI ================================

root = tk.Tk()
root.title("AWA → NMEA (VWR/MWV/VWT)")

# BooleanVarit luodaan rootin jälkeen
selected_sentences = {stype: tk.BooleanVar(value=(stype in DEFAULT_SENTENCES)) for stype in sentence_types}

canvas = tk.Canvas(root,
                   width=2*RADIUS + 2*MARGIN,
                   height=2*RADIUS + 2*MARGIN,
                   bg="white")
canvas.pack(pady=(6,0))
canvas.create_oval(center[0]-RADIUS, center[1]-RADIUS,
                   center[0]+RADIUS, center[1]+RADIUS,
                   outline="gray", width=CIRCLE_WIDTH)
draw_ticks()

readout_frame = tk.Frame(root)
readout_frame.pack(pady=(8,2))
readout_main = tk.Label(readout_frame, text="", font=FONT_MAIN)
readout_main.pack(side=tk.LEFT, padx=10)
readout_ws = tk.Label(readout_frame, text="", font=FONT_WS)
readout_ws.pack(side=tk.LEFT, padx=10)

# Tuulennopeusliukuri
ws_box = tk.LabelFrame(root, text="Wind speed", font=FONT_LBL, padx=8, pady=6)
ws_box.pack(pady=(6,4))
speed_scale = tk.Scale(ws_box, from_=SPEED_MIN, to=SPEED_MAX,
                       orient=tk.HORIZONTAL, resolution=0.1,
                       command= speed_changed, length=600, sliderlength=18)
speed_scale.set(SPEED_INIT)
speed_scale.pack()
btns = tk.Frame(ws_box)
btns.pack(pady=4)
tk.Button(btns, text="−1",   width=4, command=lambda: nudge_speed(-SPEED_STEP_COARSE)).pack(side=tk.LEFT, padx=2)
tk.Button(btns, text="−0.1", width=5, command=lambda: nudge_speed(-SPEED_STEP_FINE)).pack(side=tk.LEFT, padx=2)
tk.Button(btns, text="+0.1", width=5, command=lambda: nudge_speed(+SPEED_STEP_FINE)).pack(side=tk.LEFT, padx=2)
tk.Button(btns, text="+1",   width=4, command=lambda: nudge_speed(+SPEED_STEP_COARSE)).pack(side=tk.LEFT, padx=2)

# Checkboxit 3 sarakkeessa
sent_box = tk.LabelFrame(root, text="Sentence", font=FONT_LBL, padx=10, pady=4)
sent_box.pack(pady=(4,2), fill="x")

max_cols = 3
for i, stype in enumerate(sentence_types):
    r = i // max_cols
    c = i % max_cols
    cb = tk.Checkbutton(sent_box, text=stype, variable=selected_sentences[stype],
                        command=lambda: update_arrow(awa_deg), font=FONT_LBL)
    cb.grid(row=r, column=c, sticky="w", padx=6, pady=2)

for col in range(max_cols):
    sent_box.grid_columnconfigure(col, weight=1)

# Demotila-checkbox
demo_on = tk.BooleanVar(value=False)
demo_chk = tk.Checkbutton(root, text="Demo mode (20°/5s & 80°/20s turns)",
                          variable=demo_on, command=on_demo_toggle, font=FONT_LBL)
demo_chk.pack(pady=(4,2))

# GPS Simulator-checkbox
gps_sim_on = tk.BooleanVar(value=False)
gps_chk = tk.Checkbutton(root, text="GPS Simulator (Helsinki → SE-SW, 3–8 kn)",
                         variable=gps_sim_on, command=on_gps_toggle, font=FONT_LBL)
gps_chk.pack(pady=(2,8))

# Settings button
tk.Button(root, text="Network Settings", command=open_settings, font=FONT_LBL).pack(pady=5)

# Debug
dbg = tk.LabelFrame(root, text="Debug", font=FONT_LBL, padx=10, pady=6)
dbg.pack(pady=(2,10), fill="x")
dbg_sent = tk.Label(dbg, text="", font=FONT_DBG, justify="left", anchor="nw")
dbg_sent.pack(anchor="w", fill="x")
dbg_raw = tk.Label(dbg, text="", font=FONT_DBG)
dbg_raw.pack(anchor="w")

update_arrow(awa_deg)
canvas.bind("<B1-Motion>", mouse_move)

root.mainloop()
