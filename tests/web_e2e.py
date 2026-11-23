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

                def pixels(x, y, side=8):
                    # Sample the interior, away from the cursor outline and seams.
                    page.evaluate("() => new Promise(done => requestAnimationFrame(() => requestAnimationFrame(done)))")
                    return page.screenshot(clip={"x": x, "y": y, "width": side, "height": side})

                def click_at(x, y):
                    b = canvas.bounding_box()
                    page.mouse.click(b["x"] + x * b["width"] / 1080, b["y"] + y * b["height"] / 720)

                def saved_contains(text):
                    page.wait_for_function("text => (localStorage.getItem('patchwork.plan.v1') || '').includes(text)",
                                           arg=text, timeout=8000)

                def view_contains(text):
                    page.get_by_role("status").filter(has_text=text).wait_for()

                def download_sheet():
                    click_at(585, 36)
                    with page.expect_download(timeout=8000) as event:
                        click_at(885, 583)
                    download = event.value
                    assert download.suggested_filename == "patchwork-cutting-sheet.txt"
                    return Path(download.path()).read_text()

                open_plan(f"http://127.0.0.1:{server.server_port}/")
                honey = pixels(362, 390, 16)
                click_at(850, 228)
                click_at(370, 398)
                saved_contains("DBBBD")
                assert pixels(362, 390, 16) != honey
                click_at(899, 36)
                saved_contains("DBEBD")
                assert pixels(362, 390, 16) == honey
                click_at(995, 36)
                saved_contains("DBBBD")
                page.keyboard.press("c")
                saved_contains("5 6 4")
                open_plan()
                sheet = download_sheet()
                assert "30 squares at 4.5 x 4.5 in" in sheet and "24 x 20 in" in sheet
                assert "D B B B D A" in sheet and "Madder rose     5" in sheet

                # Preview a modern design, then apply a traditional border with the keyboard.
                click_at(715, 36)
                click_at(330, 170)
                click_at(345, 480)
                view_contains("Facet Study, Full repeat")
                click_at(790, 540)
                view_contains("Facet Study, Columns")
                saved_contains("PATCHWORK 1")
                click_at(200, 170)
                click_at(345, 480)
                click_at(790, 584)
                view_contains("Pinwheel, Border")
                page.keyboard.press("Enter")
                view_contains("Design wall. Pinwheel.")
                saved_contains("PATCHWORK 2\n5 6 4 05 6\n1DAADADDA ")

                indigo = pixels(152, 201)
                neighbor = pixels(133, 217)
                click_at(160, 205)
                saved_contains("1BAADADDA ")
                rose = pixels(152, 201)
                assert rose != indigo and pixels(133, 217) == neighbor
                click_at(899, 36)
                saved_contains("1DAADADDA ")
                assert pixels(152, 201) == indigo
                click_at(995, 36)
                saved_contains("1BAADADDA ")
                open_plan()
                view_contains("Design wall. Pinwheel.")
                assert pixels(152, 201) == rose and pixels(133, 217) == neighbor
                sheet = download_sheet()
                assert "Pinwheel / Border" in sheet and "Cut 156 pieces for 30 blocks" in sheet
                assert "Triangle, 2.854 x 2.854 in" in sheet and "[- A1]" in sheet
                assert "[R0 B2 A2" in sheet and "[R90" in sheet

                page.evaluate("localStorage.setItem('patchwork.plan.v1', 'broken')")
                open_plan()
                page.keyboard.press("Space")
                assert page.evaluate("localStorage.getItem('patchwork.plan.v1')") == "broken"
                if check_file:
                    open_plan(Path(directory, "index.html").as_uri())
                    assert "25 squares at 4.5 x 4.5 in" in download_sheet()
                assert not errors, errors
                browser.close()
        finally:
            server.shutdown()
            thread.join()
    print(f"Browser journey passed with {renderer}: squares, pattern previews, triangle paint, reload and download.")


if __name__ == "__main__":
    with tempfile.TemporaryDirectory() as directory:
        shutil.copyfile(Path(sys.argv[1], "index.html"), Path(directory, "index.html"))
        run(directory, "--also-open-file" in sys.argv[2:], "opengles2")
        run(directory, False, "software")
