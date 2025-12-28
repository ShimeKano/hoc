import gradio as gr
import pandas as pd
from matplotlib.figure import Figure

from config import UI_UPDATE_INTERVAL_SECONDS, GAS_CHART_Y_LIM, GAS_CAL_SECONDS
from ai.face_id import FaceID
from ai.gas import EnvAI
from mqtt_client import MQTTService

def make_fig(df: pd.DataFrame):
    # Dual-axis plot: Gas (left), Temp (right)
    fig = Figure(figsize=(7.2, 3.4))
    ax = fig.subplots()
    x = df["T"].values

    ax.plot(x, df["Gas"].values, color="#58a6ff", linewidth=1.6, label="Gas")
    ax.set_ylim(GAS_CHART_Y_LIM)
    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Gas (0–4095)")
    ax.grid(True, alpha=0.25)

    ax2 = ax.twinx()
    ax2.plot(x, df["Temp"].values, color="#ffa657", linewidth=1.6, label="Temp")
    ax2.set_ylabel("Temp (°C)")
    ax2.set_ylim(0, 100)

    ax.legend(loc="upper left")
    ax2.legend(loc="upper right")
    return fig

def build_app():
    face = FaceID()
    env = EnvAI()
    mqtt = MQTTService(on_metrics_message=env.handle_metrics_json)
    mqtt.start()

    css = """
    .gradio-container { background-color: #0b0f19 !important; color: white !important; }
    #card { background: #161b22 !important; border: 1px solid #30363d !important; border-radius: 12px !important; padding: 20px !important; }
    h1 { color: #58a6ff !important; text-shadow: 0 0 10px rgba(88, 166, 255, 0.3); }
    """

    with gr.Blocks() as demo:
        gr.HTML("<h1 style='text-align: center;'>🛡️ COMMAND CENTER</h1>")

        timer = gr.Timer(UI_UPDATE_INTERVAL_SECONDS)

        with gr.Row():
            with gr.Column(elem_id="card", scale=1):
                webcam = gr.Image(sources=["webcam"], type="numpy", label="Security Cam")
                btn = gr.Button("🚀 NHẬN DIỆN", variant="primary")
                name_out = gr.Textbox(label="Danh tính")
                mqtt_out = gr.Textbox(label="Lệnh IoT")

                tpl_file = gr.File(label="Tải template (.npz)", file_types=[".npz"])
                tpl_status = gr.Textbox(label="Trạng thái Template", interactive=False)

                with gr.Row():
                    blue_on  = gr.Button("💡 Bật đèn Blue")
                    blue_off = gr.Button("💡 Tắt đèn Blue")
                    door_open = gr.Button("🔓 Mở cửa 30s")
                    door_lock = gr.Button("🔒 Khóa cửa")

                gr.Markdown("### Gas IsolationForest Controls")
                with gr.Row():
                    cal_btn = gr.Button(f"🧪 Calibrate Gas ({GAS_CAL_SECONDS}s)")
                    reset_btn = gr.Button("🔄 Reset Gas IF")
                cal_status = gr.Textbox(label="IF Status", interactive=False)
                gas_if_flag = gr.Checkbox(label="IF Gas Anomaly", interactive=False)

            with gr.Column(elem_id="card", scale=2):
                plot = gr.Plot(value=make_fig(pd.DataFrame({"T": [0.0], "Gas": [0.0], "Temp": [0.0]})),
                               label="Gas & Temperature")
                with gr.Row():
                    num_out = gr.Number(label="Gas hiện tại", precision=0)
                    stat_out = gr.Textbox(label="Phân tích môi trường")
                with gr.Row():
                    red_led = gr.Checkbox(label="Gas Alert (Red)", interactive=False)
                    green_led = gr.Checkbox(label="Temp Alert (Green)", interactive=False)
                    blue_led = gr.Checkbox(label="Blue LED", interactive=False)
                    special = gr.Checkbox(label="Special Danger", interactive=False)

        # Timer tick
        def ui_tick():
            df, gas_val, summary, red, green, blue, spec, gas_if, cal_msg, calibrating = env.ui_snapshot()
            fig = make_fig(df)
            # If actively calibrating, show countdown message
            status_msg = cal_msg if cal_msg else ("⏳ Đang hiệu chuẩn..." if calibrating else "")
            return fig, gas_val, summary, bool(red), bool(green), bool(blue), bool(spec), bool(gas_if), status_msg

        timer.tick(fn=ui_tick, outputs=[plot, num_out, stat_out, red_led, green_led, blue_led, special, gas_if_flag, cal_status])

        # FaceID
        def on_recognize(frame):
            label, info, cmd = face.recognize(frame)
            if cmd is not None:
                mqtt.publish_door(cmd)
            return label, info

        btn.click(fn=on_recognize, inputs=webcam, outputs=[name_out, mqtt_out], concurrency_limit=1)

        # Template upload
        def on_template_uploaded(file):
            if file is None:
                return "❌ Chưa chọn file .npz"
            return face.set_template_file(file.name)

        tpl_file.change(fn=on_template_uploaded, inputs=tpl_file, outputs=tpl_status)

        # Blue LED controls
        blue_on.click(lambda: mqtt.publish_blue_led(True), outputs=[])
        blue_off.click(lambda: mqtt.publish_blue_led(False), outputs=[])

        # Door controls
        door_open.click(lambda: mqtt.publish_door("OPEN"), outputs=[])
        door_lock.click(lambda: mqtt.publish_door("LOCK"), outputs=[])

        # Gas IF controls
        def on_calibrate():
            return env.start_gas_calibration(GAS_CAL_SECONDS)

        def on_reset_if():
            return env.reset_gas_if()

        cal_btn.click(fn=on_calibrate, outputs=[cal_status])
        reset_btn.click(fn=on_reset_if, outputs=[cal_status])

    return demo, css