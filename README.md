# Patchwork

A design wall for a home quilter. Match five scraps from your fabric basket to the swatches, try an arrangement on screen, then take the cutting sheet to your sewing table.

[Open the quilt planner in your browser](https://stbange.github.io/patchwork/)

![The Garden Path table topper on the design wall, with five fabric swatches and quilt size controls](docs/design-wall.png)

## Try a table topper

1. Open **Patchwork**. The Garden Path sample has five rows and five columns. Each square measures 4 inches after sewing.
2. Choose **Madder rose** and click the honey-colored center. Try **Undo** and **Redo** to compare the two designs. Add a row or column if you want a larger piece.
3. Open **Cutting sheet**. For the original 20 × 20 inch topper, cut 25 squares at **4½ × 4½ inches**. The extra half inch leaves a quarter-inch seam at both edges of each dimension. Each fabric has a letter and a quantity. The row map shows where each piece goes.
4. Choose **Copy cutting sheet** on a computer and paste it into a document to print. The web version offers **Download cutting sheet** as a text file. Edits save automatically. Reopen the app in the same browser or desktop account to pick up where you left off.

The first version covers straight-sewn squares for the quilt top. Backing, batting and binding are separate materials. The design wall and cutting sheet use OpenGL on Windows and Linux and WebGL in the browser. The concept also describes an optional 3D table preview and a first sewing session to try with the quilter. [OpenSpec design](openspec/changes/archive/2026-11-22-add-quilt-design-wall/design.md)

## Run it

GitHub Pages publishes the browser version after the Linux, Windows and web tests pass on `main`. For an offline copy, download the matching artifact from a successful [build on main](https://github.com/stbange/patchwork/actions/workflows/check.yml?query=branch%3Amain).

| Platform | Open the planner |
| --- | --- |
| Windows x64 | Unzip the artifact, keep the `assets` folder beside `patchwork.exe`, and open the executable. |
| Linux x64 | Unzip the artifact, extract the tarball, and run `./patchwork` beside its `assets` folder. The binary is built on Ubuntu 24.04. |
| Web (offline) | Unzip the artifact and open `index.html` in a desktop browser. The HTML includes the application and fonts. |

The desktop app keeps the current quilt in SDL's per-user preference directory under `Patchwork/Planner/quilt.pw`. Launch `patchwork --project path/to/topper.pw` to work in a particular file. The browser saves in local storage. A damaged plan is preserved and the status bar reports the problem. On desktop, a fresh project path lets you keep working.

SDL3 chooses desktop OpenGL first, then OpenGL ES where available. The Emscripten build uses SDL's OpenGL ES 2 renderer on a WebGL canvas. Both can fall back to software rendering when GL is unavailable. The startup log reports the selected backend. On desktop, setting `SDL_RENDER_DRIVER=software` explicitly selects software rendering.

| Keyboard | Action |
| --- | --- |
| 1 through 5 | Select a fabric |
| Arrow keys, then Space | Move to a patch and place the fabric |
| C or R, with Shift to decrease | Change columns or rows |
| + or − | Change the finished size of each square |
| Ctrl+Z, Ctrl+Y or Ctrl+Shift+Z | Undo or redo |
| Tab, then Enter on the cutting sheet | Switch views and take the sheet |
| S | Retry saving after a storage error |

## Build and test

Use CMake 3.24 or newer and a C11 compiler. The build uses an installed SDL 3.2+ or fetches pinned SDL 3.2.28 sources. It also fetches a hash-checked stb_truetype header. The two open fonts and their licenses are in `assets/`. On Linux, install the X11 development packages listed in the workflow if building SDL from source.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

On Windows use Visual Studio 2022 or later. Single-configuration generators place the executable in `build/app/`. Visual Studio adds the configuration directory, such as `build/app/Release/`.

With the Emscripten 4.0.15 SDK active, build the self-contained browser file with these commands.

```sh
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build-web
```

For a fresh coverage run, use GCC and gcovr. Run the coverage command immediately after the unit suite, before launching the application or its end-to-end tests in that build. The gate counts executable lines of the project's C code in the native build. The SDL entry-point header, third-party code and test sources contribute no lines to the denominator. Browser-specific storage and download code is exercised in Chromium.

```sh
cmake -S . -B build-coverage -G Ninja -DPATCHWORK_COVERAGE=ON
cmake --build build-coverage
ctest --test-dir build-coverage --output-on-failure
gcovr --root . --filter '^src/' --txt --fail-under-line 95 build-coverage/CMakeFiles/workroom.dir
```

The desktop journey opens the real SDL window, verifies the OpenGL backend, edits a patch, checks undo and redo, adds a column, restarts the process and reads the resulting sheet from the clipboard. Linux uses Xvfb with GLX, xdotool and xclip. Windows uses Win32 window messages and the system clipboard. The Windows CI runner uses Mesa for OpenGL. The web journey checks the live WebGL context, compares pixels inside a patch before and after editing, and reads the downloaded sheet after a reload. It repeats the edit with WebGL disabled to exercise the software fallback.

```sh
xvfb-run -a -s '-screen 0 1440x940x24' python3 tests/desktop_e2e.py build/app/patchwork
# Windows: python tests/desktop_e2e.py build/app/Release/patchwork.exe
python -m pip install playwright==1.62.0
python -m playwright install chromium
python tests/web_e2e.py build-web/app
npx --yes @fission-ai/openspec@2.1.1 validate --all --strict
```

The [quilt-planning specification](openspec/specs/quilt-planning/spec.md) defines the supported behavior. The proposal, scenarios and completed checklist live in the [OpenSpec change](openspec/changes/archive/2026-11-22-add-quilt-design-wall). The C implementation follows [Ponytail](https://github.com/DietrichGebert/ponytail) by sharing one quilt model, two views and a small storage boundary.
