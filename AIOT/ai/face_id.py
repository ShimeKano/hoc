import os
import numpy as np
import torch
from PIL import Image
from facenet_pytorch import MTCNN, InceptionResnetV1
from huggingface_hub import hf_hub_download

from config import HF_REPO_ID, HF_REPO_TYPE, HF_FILENAME

class FaceID:
    def __init__(self):
        # Guarded init to avoid exceptions bubbling to UI
        try:
            self.device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
            self.mtcnn = MTCNN(image_size=160, margin=20, post_process=True, device=self.device)
            self.embedder = InceptionResnetV1(pretrained="vggface2").eval().to(self.device)
        except Exception:
            self.device = torch.device("cpu")
            self.mtcnn = None
            self.embedder = None
        self.template = None
        self.threshold = 0.85
        self._load_face_template()

    def _load_npz(self, path):
        data = np.load(path, allow_pickle=True)
        tpl = data["template"].astype(np.float32)
        tpl = tpl / (np.linalg.norm(tpl) + 1e-12)
        thr = float(data["thresh"][0])
        return tpl, thr

    def _try_load_local(self):
        candidates = [HF_FILENAME, os.path.join(os.getcwd(), HF_FILENAME), os.path.abspath(HF_FILENAME)]
        for p in candidates:
            if os.path.exists(p):
                try:
                    return self._load_npz(p)
                except Exception:
                    continue
        return None

    def _load_face_template(self):
        res = self._try_load_local()
        if res:
            self.template, self.threshold = res
            return
        try:
            path = hf_hub_download(repo_id=HF_REPO_ID, filename=HF_FILENAME, repo_type=HF_REPO_TYPE)
            self.template, self.threshold = self._load_npz(path)
        except Exception:
            self.template = None
            self.threshold = 0.85

    def set_template_file(self, file_path):
        try:
            tpl, thr = self._load_npz(file_path)
            self.template, self.threshold = tpl, thr
            return f"✅ Template đã nạp (thr={round(self.threshold,3)})"
        except Exception as e:
            return f"❌ Không đọc được .npz: {e}"

    def recognize(self, frame_np):
        # Always return strings; never raise
        try:
            if self.template is None:
                return "❌ Thiếu template", "Tải quoc_tuan_template.npz", None
            if frame_np is None:
                return "🖼️ Chưa có ảnh webcam", "Bấm Snapshot rồi nhấn NHẬN DIỆN", None
            if self.mtcnn is None or self.embedder is None:
                return "❌ Thiếu mô-đun AI", "Kiểm tra facenet_pytorch/torch", None

            img = Image.fromarray(frame_np.astype("uint8"), "RGB")
            face = self.mtcnn(img)
            if face is None:
                return "🔍 Không thấy mặt", "Thử lại", None

            with torch.no_grad():
                emb = self.embedder(face.unsqueeze(0).to(self.device))[0].cpu().numpy().astype(np.float32)

            emb = emb / (np.linalg.norm(emb) + 1e-12)
            sim = float(np.dot(emb, self.template))
            is_owner = sim >= self.threshold
            label = "✅ CHỦ NHÀ" if is_owner else "🛑 NGƯỜI LẠ"
            cmd = "OPEN" if is_owner else "LOCK"
            return label, f"📡 {cmd} ({round(sim, 2)})", cmd
        except Exception as e:
            return "❌ Lỗi nhận diện", f"Lỗi nội bộ: {e}", None