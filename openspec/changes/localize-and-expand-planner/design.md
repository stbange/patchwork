## Decisions

Use one UTF-8 translation table for controls, status messages and cutting instructions. Keep translated pattern names with their stable pattern IDs. Load an explicit preference first, then use the first supported system or browser language. English is the fallback. The native app stores the preference through SDL, and the browser stores it in local storage.

Store the finished block edge in hundredths of a millimetre. A change of display unit keeps the same edge and the same 1/4-inch seam allowance, which is exactly 0.635 cm. The unit and language preferences are independent of the quilt.

Keep polygon pieces and the five fabric slots. Give each block its own pattern and turn, and let a placement tool put a selected motif into another quilt. Add horizontal and vertical staggered seams by shifting every other band's interior joints by half a block. The shaped patches use the resulting block widths and heights for drawing, tapping, cutting and PDF output.

Use a set of ten color families and ten combinations per family. Every preset and custom hex color edits the quilt palette through Undo. A new project still begins with the original five colors.

Generate the PDF from the same cutting sheet and quilt geometry. The PDF includes a color map, searchable localized text and the measurement instructions from the text sheet. Embed the bundled font so letters such as ł and ü work across PDF viewers.

Write version 3 for the expanded quilt data and read both existing versions. For example, a 5 by 5 quilt can have Flying Geese and a Log Cabin in the same staggered row. Its PDF gives the actual piece widths for that row in the chosen units.
