import gradio as gr
import pandas as pd

from config import UI_UPDATE_INTERVAL_SECONDS, GAS_CHART_Y_LIM
from ai.face_id import FaceID
from ai.gas import GasAI
from mqtt_client import MQTTService

def build_app():
    face = FaceID()
    gas = GasAI()
    mqtt = MQTTService(on_gas_message=gas.handle_value)
    mqtt.start()

    css = """
    .gradio-container { background-color: #0b0f19 !important; color: white !important; }
    #card { background: #161b22 !important; border: 1px solid #30363d !important; border-radius: 12px !important; padding: 20px !important; }
    h1 { color: #58a6ff !important; text-shadow: 0 0 10px rgba(88, 166, 255, 0.3); }
    """

    with gr.Blocks(css=css) as demo:
        gr.HTML("<h1 style='text-align: center;'>🛡️ COMMAND CENTER</h1>")

        timer = gr.Timer(UI_UPDATE_INTERVAL_SECONDS)

        with gr.Row():
            with gr.Column(elem_id="card", scale=1):
                webcam = gr.Image(sources=["webcam"], type="numpy", label="Security Cam")
                btn = gr.Button("🚀 NHẬN DIỆN", variant="primary")
                name_out = gr.Textbox(label="Danh tính")
                mqtt_out = gr.Textbox(label="Lệnh IoT")

                # Cho phép tải template nếu thiếu trong repo
                tpl_file = gr.File(label="Tải template (.npz) nếu thiếu", file_types=[".npz"])
                tpl_status = gr.Textbox(label="Trạng thái Template", interactive=False)

            with gr.Column(elem_id="card", scale=2):
                init_df = pd.DataFrame({"T": [0.0], "G": [0.0]})
                plot = gr.LinePlot(value=init_df, x="T", y="G", y_lim=GAS_CHART_Y_LIM, height=300, title="Gas Real-time")
                with gr.Row():
                    num_out = gr.Number(label="Chỉ số Gas", precision=0)
                    stat_out = gr.Textbox(label="AI Phân tích")

        def ui_tick():
            df, gas_val, status = gas.ui_snapshot()
            return df, gas_val, status

        timer.tick(fn=ui_tick, outputs=[plot, num_out, stat_out])

        def on_recognize(frame):
            label, info, cmd = face.recognize(frame)
            if cmd is not None:
                mqtt.publish_cmd(cmd)
            return label, info

        btn.click(fn=on_recognize, inputs=webcam, outputs=[name_out, mqtt_out], concurrency_limit=1)

        def on_template_uploaded(file):
            if file is None:
                return "❌ Chưa chọn file .npz"
            return face.set_template_file(file.name)

        tpl_file.change(fn=on_template_uploaded, inputs=tpl_file, outputs=tpl_status)

    return demo