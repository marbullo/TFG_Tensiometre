import serial, time, re
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
from collections import deque
import numpy as np
from scipy import signal

# ─ CONFIG ─
PORT = "COM8"
BAUDRATE = 115200
WIN_SEC = 20
MAX_POINTS = 5000

# Regex per capturar Pasbaix i Pasbanda
regex = re.compile(
    r"Pasbaix:\s*([-+]?\d+(?:\.\d+)?)\s*mmHg\s*\|\s*Pasbanda:\s*([-+]?\d+(?:\.\d+)?)\s*V"
)

# ─ BUFFERS ─
t_buf  = deque(maxlen=MAX_POINTS)
pb_buf = deque(maxlen=MAX_POINTS)
vb_buf = deque(maxlen=MAX_POINTS)

# ─ SÈRIE ─
ser = serial.Serial(PORT, BAUDRATE, timeout=1)
time.sleep(1.5)
ser.write(b"START\n")
t0 = time.time()
print(f"Connectat a {PORT} a {BAUDRATE} bps.")

# ─ GRÀFIQUES ─
fig, (ax1, ax2) = plt.subplots(2, 1, sharex=True, figsize=(9, 6))

(line_pb,) = ax1.plot([], [], label="Pasbaix (mmHg)")
(line_vb,) = ax2.plot([], [], label="Pasbanda (V)", color="orange")

ax1.set_ylabel("Pressió (mmHg)")
ax1.grid(True)
ax1.legend()

ax2.set_ylabel("Volts (V)")
ax2.set_xlabel("Temps (s)")
ax2.grid(True)
ax2.legend()

# ─ ANIMACIÓ ─
def animate(_):
    while ser.in_waiting:
        raw = ser.readline()
        line = raw.decode(errors="ignore").strip()

        if not line:
            continue

        print(line)

        m = regex.search(line)
        if not m:
            continue

        pb = float(m.group(1))
        vb = float(m.group(2))
        t = time.time() - t0

        t_buf.append(t)
        pb_buf.append(pb)
        vb_buf.append(vb)

    if len(t_buf) >= 2:
        xmax = t_buf[-1]
        xmin = max(0, xmax - WIN_SEC)

        ax1.set_xlim(xmin, xmax)

        line_pb.set_data(t_buf, pb_buf)
        line_vb.set_data(t_buf, vb_buf)

        ax1.set_ylim(min(pb_buf) - 5, max(pb_buf) + 5)
        ax2.set_ylim(min(vb_buf) - 0.05, max(vb_buf) + 0.05)

    return line_pb, line_vb

# ─ GUARDAR + CÀLCUL ─
def on_close(_event):
    if len(t_buf) >= 2:
        t_list  = list(t_buf)
        pb_list = list(pb_buf)
        vb_list = list(vb_buf)

        fig2, (bx1, bx2) = plt.subplots(2, 1, sharex=True, figsize=(12, 7))

        bx1.plot(t_list, pb_list, label="Pasbaix (mmHg)")
        bx2.plot(t_list, vb_list, label="Pasbanda (V)", color="orange")

        bx1.grid(); bx2.grid()
        bx1.legend(); bx2.legend()

        bx2.set_xlabel("Temps (s)")

        fig2.tight_layout()
        fig2.savefig("grafica_pressio_final.png", dpi=200)
        plt.close(fig2)

        print("Gràfica guardada")

        # ───── CÀLCUL PRESSIÓ ─────
        t_np = np.array(t_list)
        pb_np = np.array(pb_list)
        vb_np = np.array(vb_list)

        # començar desinflament = màxim de pressió
        idx_ini = np.argmax(pb_np)

        pb_des = pb_np[idx_ini:]
        vb_des = vb_np[idx_ini:]

        # treure offset
        vb_des = vb_des - np.mean(vb_des)

        # pics i valls
        pics, _ = signal.find_peaks(
            vb_des,
            distance=25,
            prominence=0.02
        )

        valls, _ = signal.find_peaks(
            -vb_des,
            distance=25,
            prominence=0.02
        )

        amplituds = []
        pressions = []

        for p in pics:
            valls_prev = valls[valls < p]
            if len(valls_prev) == 0:
                continue

            v = valls_prev[-1]
            amp = vb_des[p] - vb_des[v]

            if amp > 0:
                amplituds.append(amp)
                pressions.append(pb_des[p])

        amplituds = np.array(amplituds)
        pressions = np.array(pressions)

        # filtrar rang útil
        mask = (pressions > 40) & (pressions < 180)
        amplituds = amplituds[mask]
        pressions = pressions[mask]

        if len(amplituds) > 3:
            # Buscar el MAP només en un rang vàlid de pressions i ignorant el tros inicial del desinflament
            idx_valids_map = [
                i for i, p in enumerate(pressions)
                if 60 <= p <= 130
            ]

            # Ignorem els primers punts vàlids per evitar errors
            idx_valids_map = idx_valids_map[5:]

            if len(idx_valids_map) > 0:
                idx_map = max(idx_valids_map, key=lambda i: amplituds[i])
            else:
                idx_map = np.argmax(amplituds)

            map_amp = amplituds[idx_map]
            map_pressio = pressions[idx_map]

            target_sys = 0.47 * map_amp
            target_dia = 0.72 * map_amp

            # Rang fisiològic segons MAP per evitar falsos pics inicials
            rang_sys_min = map_pressio + 10
            rang_sys_max = map_pressio + 70

            rang_dia_min = 40
            rang_dia_max = map_pressio - 5

            # SISTÒLICA: abans del MAP però descartant pressions  altes
            idx_sys = None
            for i in range(idx_map):
                if rang_sys_min <= pressions[i] <= rang_sys_max:
                    if amplituds[i] >= target_sys:
                        idx_sys = i
                        break

            # DIASTÒLICA: després del MAP
            idx_dia = None
            for i in range(idx_map + 1, len(amplituds)):
                if rang_dia_min <= pressions[i] <= rang_dia_max:
                    if amplituds[i] <= target_dia:
                        idx_dia = i
                        break

            if idx_sys is not None and idx_dia is not None:

                sistolica = int(pressions[idx_sys])
                diastolica = int(pressions[idx_dia])

                # enviar a ESP32
                missatge = f"RESULTATS:{sistolica},{diastolica}\n"

                ser.write(missatge.encode())
                ser.flush()

                print("Resultats enviats")
                time.sleep(1)

            print("\n--- RESULTATS ---")
            print(f"MAP: {map_pressio:.2f} mmHg")

            if idx_sys is not None:
                print(f"Sistòlica: {pressions[idx_sys]:.2f} mmHg")
            else:
                print("Sistòlica no trobada")

            if idx_dia is not None:
                print(f"Diastòlica: {pressions[idx_dia]:.2f} mmHg")
            else:
                print("Diastòlica no trobada")

        else:
            print("No hi ha prou dades")


    ser.close()


fig.canvas.mpl_connect("close_event", on_close)

ani = FuncAnimation(fig, animate, interval=50, cache_frame_data=False)

plt.tight_layout()
plt.show()