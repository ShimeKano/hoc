# Entry point used by Hugging Face Spaces (app_file = app.py)
from ui import build_app

demo = build_app()

if __name__ == "__main__":
    demo.launch()