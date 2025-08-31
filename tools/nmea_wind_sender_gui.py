import tkinter as tk
import math
import socket
import random

# ============================ ASETUKSET ============================
ESP_IP = "192.168.68.119"
ESP_PORT = 10110

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

DEFAULT_SENTENCE = "VWR"
AWA_INIT = 45.0
# ===================================================================

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

center = (RADIUS + MARGIN, RADIUS + MARGIN)
awa_deg = AWA_INIT
speed_kn = SPEED_INIT
sentence_type = DEFAULT_SENTENCE

# Demotila
demo_on = None
_demo_side_right = True
_demo_base_angle = 45.0
_demo_tick_count = 0

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
    a360 = angle_deg % 360
    if stype == "VWR":
        mag, side = to_vwr_components(a360)
        payload = f"IIVWR,{mag:.1f},{side},{speed_kn:.1f},N,,M,,K"
    elif stype == "VWT":
        mag, side = to_vwr_components(a360)
        payload = f"IIVWT,{mag:.1f},{side},{speed_kn:.1f},N,,M,,K"
    elif stype == "MWV(R)":
        payload = f"IIMWV,{a360:.1f},R,{speed_kn:.1f},N,A"
    elif stype == "MWV(T)":
        payload = f"IIMWV,{a360:.1f},T,{speed_kn:.1f},N,A"
    else:
        mag, side = to_vwr_components(a360)
        payload = f"IIVWR,{mag:.1f},{side},{speed_kn:.1f},N,,M,,K"
    return f"${payload}*{calculate_checksum(payload)}\r\n"

def send_nmea(angle_deg, speed_kn):
    line = build_sentence(angle_deg, speed_kn, sentence_type)
    sock.sendto(line.encode(), (ESP_IP, ESP_PORT))
    return line.strip()
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

    nmea_line = send_nmea(awa_deg, speed_kn)

    if sentence_type in ("VWR", "VWT"):
        readout_main.config(text=f"{sentence_type}: {mag_vwr:.1f}° {side_vwr}")
    elif sentence_type == "MWV(R)":
        readout_main.config(text=f"MWV(R): {awa_deg:.1f}° R")
    elif sentence_type == "MWV(T)":
        readout_main.config(text=f"MWV(T): {awa_deg:.1f}° T")
    else:
        readout_main.config(text=f"{sentence_type}: {mag_vwr:.1f}° {side_vwr}")

    readout_ws.config(text=f"WS: {speed_kn:.1f} kn")
    dbg_sent.config(text=f"Sent: {nmea_line}")
    dbg_raw.config(text=f"AWA raw: {awa_deg:.1f}°")

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

def change_sentence():
    global sentence_type
    sentence_type = sentence_var.get()
    update_arrow(awa_deg)

# ============================ DEMOTILA ============================
def on_demo_toggle():
    if demo_on.get():
        demo_tick()

def demo_tick():
    global awa_deg, speed_kn, _demo_tick_count

    if not demo_on.get():
        return

    # Every 20s: 80° turn, random direction
    if _demo_tick_count % 100 == 0:
        direction80 = random.choice([-1, 1])
        demo_tick.target = (awa_deg + direction80 * 80) % 360
        demo_tick.start = awa_deg
        demo_tick.steps = 15  # 3s fast transition
        demo_tick.step = 0
    # Every 5s: 20° turn, random direction (only if not in 80° turn)
    elif _demo_tick_count % 25 == 0:
        direction20 = random.choice([-1, 1])
        demo_tick.target = (awa_deg + direction20 * 20) % 360
        demo_tick.start = awa_deg
        demo_tick.steps = 25  # 5s smooth transition
        demo_tick.step = 0

    # Interpolate turn if in progress
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

# ============================== UI ================================
root = tk.Tk()
root.title("AWA → NMEA (VWR/MWV/VWT)")

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
                       command=speed_changed, length=600, sliderlength=18)
speed_scale.set(SPEED_INIT)
speed_scale.pack()
btns = tk.Frame(ws_box)
btns.pack(pady=4)
tk.Button(btns, text="−1", width=4, command=lambda: nudge_speed(-SPEED_STEP_COARSE)).pack(side=tk.LEFT, padx=2)
tk.Button(btns, text="−0.1", width=5, command=lambda: nudge_speed(-SPEED_STEP_FINE)).pack(side=tk.LEFT, padx=2)
tk.Button(btns, text="+0.1", width=5, command=lambda: nudge_speed(+SPEED_STEP_FINE)).pack(side=tk.LEFT, padx=2)
tk.Button(btns, text="+1", width=4, command=lambda: nudge_speed(+SPEED_STEP_COARSE)).pack(side=tk.LEFT, padx=2)

# Radiobuttonit kahteen sarakkeeseen automaattisesti
sent_box = tk.LabelFrame(root, text="Sentence", font=FONT_LBL, padx=10, pady=4)
sent_box.pack(pady=(4,2), fill="x")

sentence_var = tk.StringVar(value=DEFAULT_SENTENCE)
labels = ("VWR", "MWV(R)", "MWV(T)", "VWT")

rows = math.ceil(len(labels) / 2)
for i, label in enumerate(labels):
    r = i % rows
    c = i // rows
    rb = tk.Radiobutton(sent_box, text=label, variable=sentence_var, value=label,
                        command=change_sentence, font=FONT_LBL)
    rb.grid(row=r, column=c, sticky="w", padx=6, pady=2)

sent_box.grid_columnconfigure(0, weight=1)
sent_box.grid_columnconfigure(1, weight=1)

# Demotila-checkbox
demo_on = tk.BooleanVar(value=False)
demo_chk = tk.Checkbutton(root, text="Demo mode (20° turn every 5s, 80° turn every 20s)",
                          variable=demo_on, command=on_demo_toggle, font=FONT_LBL)
demo_chk.pack(pady=(4,8))

# Debug
dbg = tk.LabelFrame(root, text="Debug", font=FONT_LBL, padx=10, pady=6)
dbg.pack(pady=(2,10), fill="x")
dbg_sent = tk.Label(dbg, text="", font=FONT_DBG)
dbg_sent.pack(anchor="w")
dbg_raw = tk.Label(dbg, text="", font=FONT_DBG)
dbg_raw.pack(anchor="w")

update_arrow(awa_deg)
canvas.bind("<B1-Motion>", mouse_move)

root.mainloop()
