"""Exercise the compiled WebAssembly app through Chromium and its real download."""
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import os
import shutil
import tempfile
from pathlib import Path
import sys
from threading import Thread

from playwright.sync_api import sync_playwright


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass


def run(directory, check_file):
    with ThreadingHTTPServer(("127.0.0.1", 0), partial(QuietHandler, directory=directory)) as server:
        thread = Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            with sync_playwright() as pw:
                browser = pw.chromium.launch(executable_path=os.getenv("CHROMIUM_PATH"))
                page = browser.new_page(viewport={"width": 1080, "height": 720}, accept_downloads=True)
                errors = []
                page.on("pageerror", lambda error: errors.append(str(error)))
                page.goto(f"http://127.0.0.1:{server.server_port}/")
                canvas = page.locator("#canvas")

                def ready():
                    page.locator("#loading").wait_for(state="detached")
                    canvas.wait_for()

                def click_at(x, y):
                    b = canvas.bounding_box()
                    page.mouse.click(b["x"] + x * b["width"] / 1080, b["y"] + y * b["height"] / 720)

                def saved_contains(text):
                    page.wait_for_function("text => (localStorage.getItem('patchwork.plan.v1') || '').includes(text)",
                                           arg=text, timeout=8000)

                ready()
                click_at(850, 228)
                click_at(370, 398)
                saved_contains("DBBBD")
                click_at(899, 36)
                saved_contains("DBEBD")
                click_at(995, 36)
                saved_contains("DBBBD")
                page.keyboard.press("c")
                saved_contains("5 6 4")
                page.reload()
                ready()
                click_at(585, 36)
                with page.expect_download(timeout=8000) as event:
                    click_at(885, 583)
                download = event.value
                assert download.suggested_filename == "patchwork-cutting-sheet.txt"
                sheet = Path(download.path()).read_text()
                assert "30 squares at 4.5 x 4.5 in" in sheet and "24 x 20 in" in sheet
                assert "D B B B D A" in sheet and "Madder rose     5" in sheet
                page.evaluate("localStorage.setItem('patchwork.plan.v1', 'broken')")
                page.reload()
                ready()
                page.keyboard.press("Space")
                assert page.evaluate("localStorage.getItem('patchwork.plan.v1')") == "broken"
                if check_file:
                    page.goto(Path(directory, "index.html").as_uri())
                    ready()
                    click_at(585, 36)
                    with page.expect_download(timeout=8000) as event:
                        click_at(885, 583)
                    assert "25 squares at 4.5 x 4.5 in" in Path(event.value.path()).read_text()
                assert not errors, errors
                browser.close()
        finally:
            server.shutdown()
            thread.join()
    print("Browser journey passed: paint, undo, redo, resize, reload, download, preserve a damaged plan.")


if __name__ == "__main__":
    with tempfile.TemporaryDirectory() as directory:
        shutil.copyfile(Path(sys.argv[1], "index.html"), Path(directory, "index.html"))
        run(directory, "--also-open-file" in sys.argv[2:])
