import threading
import time
import json
import numpy as np
import pandas as pd
from collections import deque
from sklearn.ensemble import IsolationForest

from config import GAS_CHART_MAX_POINTS, GAS_IF_CONTAMINATION, GAS_CAL_SECONDS

class EnvAI:
    """
    Holds Gas/Temp metrics and Space-side IsolationForest for gas.
    Wide DataFrame: columns T, Gas, Temp for robust plotting.
    """
    def __init__(self):
        # Space-side IsolationForest for gas (trained when you calibrate)
        self.gas_if = None  # type: IsolationForest | None
        self.gas_if_contam = GAS_IF_CONTAMINATION

        # Optional temp IF (kept simple; not exposed in UI here)
        self.temp_if = IsolationForest(contamination=0.05, random_state=42)
        self.temp_if.fit(np.random.normal(30.0, 3.0, (500, 1)))

        self.lock = threading.Lock()
        self.start_time = time.time()
        self.df = pd.DataFrame({"T": [0.0], "Gas": [0.0], "Temp": [0.0]})

        # Live values
        self.gas_val = 0.0
        self.temp_val = 0.0
        self.hum_val = 0.0
        self.pir = 0
        self.red = 0
        self.green = 0
        self.blue = 0
        self.vent = 0
        self.special = 0

        # Calibration state
        self.calibrating = False
        self.cal_end_t = 0.0
        self.cal_samples = deque()  # gas samples during calibration

    def handle_metrics_json(self, payload: str):
        try:
            m = json.loads(payload)
        except Exception:
            return

        now = round(time.time() - self.start_time, 1)
        gas = float(m.get("gas", 0.0))
        temp = float(m.get("temp", 0.0))
        hum = float(m.get("hum", 0.0))

        with self.lock:
            # Update live states
            self.gas_val = gas
            self.temp_val = temp
            self.hum_val = hum
            self.pir = int(m.get("pir", 0))
            self.red = int(m.get("red", 0))
            self.green = int(m.get("green", 0))
            self.blue = int(m.get("blue", 0))
            self.vent = int(m.get("vent", 0))
            self.special = int(m.get("special", 0))

            # Append to DF
            new_row = pd.DataFrame({"T": [now], "Gas": [gas], "Temp": [temp]})
            self.df = pd.concat([self.df, new_row], ignore_index=True).iloc[-GAS_CHART_MAX_POINTS:]

            # Collect calibration samples if active
            if self.calibrating:
                self.cal_samples.append([gas])
                if time.time() >= self.cal_end_t:
                    # Train IsolationForest on collected samples
                    try:
                        X = np.array(self.cal_samples, dtype=np.float32)
                        if len(X) >= 32:  # minimal sample size
                            self.gas_if = IsolationForest(
                                contamination=self.gas_if_contam,
                                random_state=42
                            ).fit(X)
                            status = f"✅ Gas IF calibrated on {len(X)} samples (contam={self.gas_if_contam})"
                        else:
                            self.gas_if = None
                            status = f"❌ Not enough samples for IF ({len(X)})"
                    except Exception as e:
                        self.gas_if = None
                        status = f"❌ IF training error: {e}"
                    # Reset calibration state
                    self.calibrating = False
                    self.cal_samples.clear()
                    self._last_cal_status = status

    def temp_anomaly(self):
        try:
            pred = self.temp_if.predict([[self.temp_val]])
            return pred[0] == -1
        except Exception:
            return False

    def gas_anomaly(self):
        """Predict anomaly with gas IsolationForest if available."""
        try:
            if self.gas_if is None:
                return False
            pred = self.gas_if.predict([[self.gas_val]])
            return pred[0] == -1
        except Exception:
            return False

    def start_gas_calibration(self, seconds: int = GAS_CAL_SECONDS):
        with self.lock:
            self.calibrating = True
            self.cal_end_t = time.time() + max(2, int(seconds))
            self.cal_samples.clear()
            self._last_cal_status = f"⏱️ Calibrating gas baseline for {seconds}s..."
            return self._last_cal_status

    def reset_gas_if(self):
        with self.lock:
            self.gas_if = None
            self._last_cal_status = "🔄 Gas IsolationForest reset."
            return self._last_cal_status

    def ui_snapshot(self):
        with self.lock:
            # Build summary
            summary = f"Gas={self.gas_val:.0f} | Temp={self.temp_val:.1f}C | Hum={self.hum_val:.1f}% | PIR={self.pir} | Vent={'ON' if self.vent else 'OFF'}"
            if self.special:
                summary += " | 🔴 SPECIAL DANGER (Gas+Temp)"
            # IF status
            cal_status = getattr(self, "_last_cal_status", "")
            gas_if_flag = self.gas_anomaly()
            return self.df.copy(), self.gas_val, summary, self.red, self.green, self.blue, self.special, gas_if_flag, cal_status, self.calibrating