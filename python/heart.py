import serial
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque
import time

# Seri port ayarları
SERIAL_PORT = 'COM10'
BAUD_RATE = 9600

# BPM verileri
bpm_data = deque()
bpm_log = deque()

# Seri bağlantı
ser = serial.Serial(
    SERIAL_PORT,
    BAUD_RATE,
    timeout=1
)

# Grafik
fig, ax = plt.subplots()
line, = ax.plot([], [], label="BPM")
ax.set_ylim(0, 200)
ax.set_title("Canlı BPM Grafiği (Zaman Bazlı)")
ax.set_xlabel("Zaman (saniye)")
ax.set_ylabel("BPM")
ax.grid(True)
ax.legend()

instant_bpm_text = ax.text(
    0.6, 0.9, "",
    transform=ax.transAxes,
    fontsize=12,
    color='red',
    bbox=dict(facecolor='white', edgecolor='red')
)

avg_bpm_text = ax.text(
    0.6, 0.82, "",
    transform=ax.transAxes,
    fontsize=12,
    color='blue',
    bbox=dict(facecolor='white', edgecolor='blue')
)

start_time = time.time()
window_size = 30  # saniye


def update(frame):
    try:
        line_raw = ser.readline().decode('utf-8').strip()

        if line_raw.startswith("BPM"):
            bpm_value = int(line_raw.split(":")[1].strip())
            current_time = time.time() - start_time

            # Veriyi kaydet
            bpm_data.append((current_time, bpm_value))
            bpm_log.append((current_time, bpm_value))

            # Ortalama için pencereyi temizle
            while bpm_log and current_time - bpm_log[0][0] > window_size:
                bpm_log.popleft()

            # Tüm veriler grafik için kullanılsın
            x_vals = [t for t, _ in bpm_data]
            y_vals = [v for _, v in bpm_data]
            line.set_data(x_vals, y_vals)

            ax.set_xlim(max(0, current_time - window_size), current_time)

            instant_bpm_text.set_text(f"Anlık BPM: {bpm_value}")

            if bpm_log:
                avg = sum(val for _, val in bpm_log) / len(bpm_log)
                avg_bpm_text.set_text(f"Ortalama: {int(avg)}")

    except:
        pass

    return line, instant_bpm_text, avg_bpm_text


ani = animation.FuncAnimation(
    fig,
    update,
    interval=200
)

plt.tight_layout()
plt.show()
