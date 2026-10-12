# Module `export`

Namespace `rc::exporting` (`export` is a C++ keyword).

* `Drawing` — 2D vector drawing (layers, lines, polylines, circles, texts, dimensions); the common model behind every output.
* `PartDrawing` — a part from `detailing::FlatPart`: contour, edge bands, holes and grooves on layers named after tool and depth (`DRILL_D5_Z13`); all parts in one sheet or one DXF per part.
* `ProductViews` — front, open front and section A-A with hidden lines removed and overall/assembly dimensions.
* `Dxf` — DXF R12 writer.
* `Paper`, `Documentation` — pages (A4-A1, portrait/landscape) with frame and title block: summary, cut list, edge banding + hardware, assembly drawing, part drawings.
* `Pdf` — PDF writer on libHaru (private dependency), TrueType embedding, font metrics.
* `SheetDrawing` — sheet layouts of the cutting plan (stage 7): parts turned and moved to their place with their machining, remnants, saw cuts; one DXF per layout and the "Kesim planı" pages of the PDF.

Stage 9: cut list rows left out of the cutting plan are marked with "*" and a footnote; `todayText()`.

**Stages:** 6, 7, 9
