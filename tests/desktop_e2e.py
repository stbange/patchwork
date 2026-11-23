"""Drive the packaged SDL window with the OS, then read its cutting sheet."""
import ctypes
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time


def until(read, description):
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        result = read()
        if result:
            return result
        time.sleep(0.1)
    raise AssertionError(f"Timed out waiting for {description}")


if sys.platform == "win32":
    from ctypes import wintypes

    user32 = ctypes.windll.user32
    user32.FindWindowW.restype = wintypes.HWND
    user32.FindWindowW.argtypes = [wintypes.LPCWSTR, wintypes.LPCWSTR]
    user32.PostMessageW.argtypes = [wintypes.HWND, wintypes.UINT, wintypes.WPARAM, wintypes.LPARAM]
    user32.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
    user32.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]

    def window_for(pid):
        window = user32.FindWindowW(None, "Patchwork")
        owner = wintypes.DWORD()
        user32.GetWindowThreadProcessId(window, ctypes.byref(owner))
        return window if owner.value == pid else None

    def click(window, x, y):
        rect = wintypes.RECT()
        assert user32.GetClientRect(window, ctypes.byref(rect))
        scale = min(rect.right / 1080, rect.bottom / 720)
        px = round((rect.right - 1080 * scale) / 2 + x * scale)
        py = round((rect.bottom - 720 * scale) / 2 + y * scale)
        pos = px | (py << 16)
        assert user32.PostMessageW(window, 0x0200, 0, pos)  # WM_MOUSEMOVE
        assert user32.PostMessageW(window, 0x0201, 1, pos)  # WM_LBUTTONDOWN
        assert user32.PostMessageW(window, 0x0202, 0, pos)  # WM_LBUTTONUP

    def close(window, process):
        assert user32.PostMessageW(window, 0x0010, 0, 0)  # WM_CLOSE
        assert process.wait(timeout=10) == 0

    def clipboard():
        return subprocess.run(["powershell", "-NoProfile", "-NonInteractive",
                               "-Command", "Get-Clipboard -Raw"], capture_output=True,
                              text=True, timeout=8).stdout
else:
    def window_for(pid):
        result = subprocess.run(["xdotool", "search", "--all", "--onlyvisible",
                                 "--pid", str(pid), "--name", "^Patchwork$"],
                                capture_output=True, text=True)
        return next(iter(result.stdout.splitlines()), None)

    def click(window, x, y):
        subprocess.run(["xdotool", "mousemove", "--window", window, str(x), str(y),
                        "click", "1"], check=True)

    def close(window, process):
        process.terminate()
        process.wait(timeout=10)

    def clipboard():
        return subprocess.run(["xclip", "-o", "-selection", "clipboard"],
                              capture_output=True, text=True, timeout=3).stdout


def run(executable):
    with tempfile.TemporaryDirectory() as temp:
        plan = Path(temp, "topper.pw")
        log = Path(temp, "startup.log")
        env = os.environ.copy()
        env.pop("SDL_RENDER_DRIVER", None)
        if sys.platform != "win32":
            env["SDL_VIDEO_DRIVER"] = "x11"
        for visit in range(3):
            with log.open("wb") as output:
                process = subprocess.Popen([executable, "--project", str(plan)], env=env,
                                           stdout=output, stderr=subprocess.STDOUT)
            try:
                def ready():
                    assert process.poll() is None, log.read_text(errors="replace")
                    return window_for(process.pid)
                window = until(ready, "the design wall")
                if visit == 0:
                    click(window, 850, 228)  # Madder rose
                    click(window, 370, 398)  # Center patch
                    until(lambda: plan.exists() and "DBBBD" in plan.read_text(), "the rose center")
                    click(window, 899, 36)
                    until(lambda: "DBEBD" in plan.read_text(), "undo")
                    click(window, 995, 36)
                    until(lambda: "DBBBD" in plan.read_text(), "redo")
                    click(window, 1020, 492)
                    until(lambda: "5 6 4" in plan.read_text(), "six columns")
                else:
                    click(window, 585, 36)
                    click(window, 885, 583)
                    def copied_sheet():
                        text = clipboard()
                        expected = "30 squares at 4.5 x 4.5 in" if visit == 1 else "Flying Geese / Columns"
                        return text if expected in text else None
                    text = until(copied_sheet, "the copied cutting sheet")
                    assert "24 x 20 in" in text
                    if visit == 1:
                        assert "D B B B D A" in text and "Madder rose     5" in text
                        click(window, 715, 36)  # Patterns
                        click(window, 200, 170)  # Traditional
                        click(window, 247, 638)  # Next
                        click(window, 345, 285)  # Flying Geese
                        click(window, 790, 540)  # Columns
                        assert plan.read_text().startswith("PATCHWORK 1")
                        click(window, 875, 636)  # Apply
                        until(lambda: "5 6 4 08 4\n2DAABAA " in plan.read_text(), "the rotated geese")
                        click(window, 189, 235)  # Paint just the first goose
                        until(lambda: "5 6 4 08 4\n2BAABAA " in plan.read_text(), "the painted goose")
                    else:
                        assert "Cut 105 pieces for 30 blocks" in text
                        assert "Triangle" in text and "[R90 B2" in text and "[- A1]" in text
                close(window, process)
                startup = log.read_text(errors="replace")
                assert "Patchwork renderer: opengl" in startup.splitlines(), startup
            finally:
                if process.poll() is None:
                    process.kill()
                    process.wait()
    print("Desktop journey passed: squares, pattern selection, rotated triangle paint, reopen and copy.")


if __name__ == "__main__":
    run(str(Path(sys.argv[1]).resolve()))
