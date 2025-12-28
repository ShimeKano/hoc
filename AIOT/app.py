from ui import build_app

demo, css = build_app()

if __name__ == "__main__":
    demo.launch(css=css)