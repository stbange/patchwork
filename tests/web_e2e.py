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


def run(directory, check_file, renderer):
    with ThreadingHTTPServer(("127.0.0.1", 0), partial(QuietHandler, directory=directory)) as server:
        thread = Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            with sync_playwright() as pw:
                args = ["--disable-webgl"] if renderer == "software" else []
                browser = pw.chromium.launch(executable_path=os.getenv("CHROMIUM_PATH"), args=args)
                page = browser.new_page(viewport={"width": 1080, "height": 720}, accept_downloads=True)
                errors = []
                page.on("pageerror", lambda error: errors.append(str(error)))
                canvas = page.locator("#canvas")

                def open_plan(url=None):
                    with page.expect_console_message(lambda msg: msg.text == f"Patchwork renderer: {renderer}"):
                        if url is None:
                            page.reload()
                        else:
                            page.goto(url)
                    if renderer == "opengles2":
                        assert canvas.evaluate("""canvas => {
                            const gl = canvas.getContext('webgl') || canvas.getContext('webgl2');
                            return gl && !gl.isContextLost();
                        }""")

                def center_patch():
                    # Sample the interior, away from the cursor outline and seams.
                    page.evaluate("() => new Promise(done => requestAnimationFrame(() => requestAnimationFrame(done)))")
                    return page.screenshot(clip={"x": 362, "y": 390, "width": 16, "height": 16})

                def click_at(x, y):
                    b = canvas.bounding_box()
                    page.mouse.click(b["x"] + x * b["width"] / 1080, b["y"] + y * b["height"] / 720)

                def saved_contains(text):
                    page.wait_for_function("text => (localStorage.getItem('patchwork.plan.v1') || '').includes(text)",
                                           arg=text, timeout=8000)

                open_plan(f"http://127.0.0.1:{server.server_port}/")
                honey = center_patch()
                click_at(850, 228)
                click_at(370, 398)
                saved_contains("DBBBD")
                assert center_patch() != honey
                click_at(899, 36)
                saved_contains("DBEBD")
                assert center_patch() == honey
                click_at(995, 36)
                saved_contains("DBBBD")
                page.keyboard.press("c")
                saved_contains("5 6 4")
                open_plan()
                click_at(585, 36)
                with page.expect_download(timeout=8000) as event:
                    click_at(885, 583)
                download = event.value
                assert download.suggested_filename == "patchwork-cutting-sheet.txt"
                sheet = Path(download.path()).read_text()
                assert "30 squares at 4.5 x 4.5 in" in sheet and "24 x 20 in" in sheet
                assert "D B B B D A" in sheet and "Madder rose     5" in sheet
                page.evaluate("localStorage.setItem('patchwork.plan.v1', 'broken')")
                open_plan()
                page.keyboard.press("Space")
                assert page.evaluate("localStorage.getItem('patchwork.plan.v1')") == "broken"
                if check_file:
                    open_plan(Path(directory, "index.html").as_uri())
                    click_at(585, 36)
                    with page.expect_download(timeout=8000) as event:
                        click_at(885, 583)
                    assert "25 squares at 4.5 x 4.5 in" in Path(event.value.path()).read_text()
                assert not errors, errors
                browser.close()
        finally:
            server.shutdown()
            thread.join()
    print(f"Browser journey passed with {renderer}: paint, undo, redo, resize, reload and download.")


if __name__ == "__main__":
    with tempfile.TemporaryDirectory() as directory:
        shutil.copyfile(Path(sys.argv[1], "index.html"), Path(directory, "index.html"))
        run(directory, "--also-open-file" in sys.argv[2:], "opengles2")
        run(directory, False, "software")
