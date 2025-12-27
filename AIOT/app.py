import gradio as gr
import pandas as pd
import numpy as np
import paho.mqtt.client as mqtt
from paho.mqtt.enums import CallbackAPIVersion
import threading
import time
import torch
from PIL import Image
from facenet_pytorch import MTCNN, InceptionResnetV1
from huggingface_hub import hf_hub_download
from sklearn.ensemble import IsolationForest

# --- 1. CẤU HÌNH & DỮ LIỆU ---
REPO_ID = "balenkano/datasetface" 
FILENAME = "quoc_tuan_template.npz"
MQTT_BROKER = "test.mosquitto.org"

# State lưu trữ dữ liệu tập trung để giảm tải trình duyệt
state = {
    "gas_val": 0,
    "gas_status": "Ổn định",
    "df": pd.DataFrame(columns=["T", "G"]),
    "start_time": time.time()
}

# --- 2. AI GAS & FACE ID ---
gas_model = IsolationForest(contamination=0.05)
gas_model.fit(np.random.normal(2500, 500, (500, 1))) 

device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
mtcnn = MTCNN(image_size=160, margin=20, post_process=True, device=device)
embedder = InceptionResnetV1(pretrained='vggface2').eval().to(device)

def load_face_data():
    try:
        path = hf_hub_download(repo_id=REPO_ID, filename=FILENAME, repo_type="dataset")
        data = np.load(path, allow_pickle=True)
        return data['template'].astype(np.float32), float(data['thresh'][0])
    except: return None, 0.85

template_data, threshold_val = load_face_data()

# --- 3. MQTT LOGIC (Tối ưu để không gây lag nút bấm) ---
def on_message(client, userdata, message):
    try:
        val = float(message.payload.decode())
        state["gas_val"] = val
        pred = gas_model.predict([[val]])
        state["gas_status"] = "⚠️ NGUY HIỂM" if pred[0] == -1 else "✅ AN TOÀN"
        
        # Chỉ giữ 15 điểm để biểu đồ không làm lag web
        now = round(time.time() - state["start_time"], 1)
        new_row = pd.DataFrame({"T": [now], "G": [val]})
        state["df"] = pd.concat([state["df"], new_row]).iloc[-15:]
    except: pass

def start_mqtt():
    c = mqtt.Client(CallbackAPIVersion.VERSION2)
    c.on_message = on_message
    c.connect(MQTT_BROKER, 1883)
    c.subscribe("balenkano_tuan_gas_data")
    c.loop_start()

threading.Thread(target=start_mqtt, daemon=True).start()

# --- 4. HÀM XỬ LÝ (SỬA LỖI NÚT NHẬN DIỆN) ---
def process_recognition(frame):
    if frame is None or template_data is None: return "❌ Lỗi dữ liệu", "N/A"
    img = Image.fromarray(frame.astype('uint8'), 'RGB')
    face = mtcnn(img)
    if face is None: return "🔍 Không thấy mặt", "Thử lại"
    
    with torch.no_grad():
        emb = embedder(face.unsqueeze(0).to(device))[0].cpu().numpy()
    
    emb = emb / (np.linalg.norm(emb) + 1e-12)
    sim = float(np.dot(emb, template_data))
    
    label = "✅ CHỦ NHÀ" if sim >= threshold_val else "🛑 NGƯỜI LẠ"
    cmd = "OPEN" if sim >= threshold_val else "LOCK"
    
    # Gửi lệnh MQTT xuống Wokwi
    threading.Thread(target=lambda: mqtt.Client(CallbackAPIVersion.VERSION2).connect(MQTT_BROKER, 1883).publish("balenkano_tuan_door_2025", cmd)).start()
    return label, f"📡 Gửi {cmd} ({round(sim, 2)})"

# --- 5. GIAO DIỆN CSS DARK MODE ---
css = """
.gradio-container { background-color: #0b0f19 !important; color: white !important; }
#card { background: #161b22 !important; border: 1px solid #30363d !important; border-radius: 12px !important; padding: 20px !important; }
h1 { color: #58a6ff !important; text-shadow: 0 0 10px rgba(88, 166, 255, 0.3); }
"""

with gr.Blocks(css=css) as demo:
    gr.HTML("<h1 style='text-align: center;'>🛡️ COMMAND CENTER v2.0</h1>")
    
    # Tăng Timer lên 3 giây để máy bạn bớt lag
    timer = gr.Timer(3)
    
    with gr.Row():
        with gr.Column(elem_id="card", scale=1):
            webcam = gr.Image(sources=["webcam"], type="numpy", label="Security Cam")
            # concurrency_limit=1 để ưu tiên nút bấm hơn biểu đồ
            btn = gr.Button("🚀 NHẬN DIỆN", variant="primary")
            name_out = gr.Textbox(label="Danh tính")
            mqtt_out = gr.Textbox(label="Lệnh IoT")
            
        with gr.Column(elem_id="card", scale=2):
            plot = gr.LinePlot(value=state["df"], x="T", y="G", y_lim=[0, 4095], height=300, title="Gas Real-time")
            with gr.Row():
                num_out = gr.Number(label="Chỉ số Gas", precision=0)
                stat_out = gr.Textbox(label="AI Phân tích")

    # Cập nhật UI định kỳ
    timer.tick(lambda: [state["df"], state["gas_val"], state["gas_status"]], outputs=[plot, num_out, stat_out])
    
    # Xử lý nút bấm
    btn.click(fn=process_recognition, inputs=webcam, outputs=[name_out, mqtt_out], concurrency_limit=1)

demo.launch()