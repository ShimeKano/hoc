import threading
import time
import numpy as np
import pandas as pd
from sklearn.ensemble import IsolationForest

from config import GAS_CHART_MAX_POINTS

class GasAI:
    def __init__(self):
        self.model = IsolationForest(contamination=0.05, random_state=42)
        self.model.fit(np.random.normal(2500, 500, (500, 1)))

        self.lock = threading.Lock()
        self.start_time = time.time()
        self.df = pd.DataFrame({"T": [0.0], "G": [0.0]})
        self.gas_val = 0.0
        self.gas_status = "Ổn định"

    def handle_value(self, val_float: float):
        pred = self.model.predict([[val_float]])
        status = "⚠️ NGUY HIỂM" if pred[0] == -1 else "✅ AN TOÀN"

        now = round(time.time() - self.start_time, 1)
        new_row = pd.DataFrame({"T": [now], "G": [val_float]})

        with self.lock:
            self.gas_val = val_float
            self.gas_status = status
            self.df = pd.concat([self.df, new_row], ignore_index=True).iloc[-GAS_CHART_MAX_POINTS:]

        return status

    def ui_snapshot(self):
        with self.lock:
            return self.df.copy(), self.gas_val, self.gas_status