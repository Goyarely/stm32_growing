import sys
import pathlib
import fitz  # PyMuPDF

OUT = pathlib.Path(r'd:\02_Project_Source\STM32_0914\my_project\_sch_png')
OUT.mkdir(exist_ok=True)

for src in sys.argv[1:]:
    p = pathlib.Path(src)
    doc = fitz.open(src)
    page = doc[0]
    r = page.rect
    tag = p.stem.replace('SCH_', '').split('_')[0]

    pm = page.get_pixmap(dpi=170)
    pm.save(str(OUT / (tag + '_full.png')))
    print('full', tag, pm.width, pm.height)

    cols, rows, dpi = 2, 2, 360
    for iy in range(rows):
        for ix in range(cols):
            clip = fitz.Rect(
                r.x0 + r.width * ix / cols,
                r.y0 + r.height * iy / rows,
                r.x0 + r.width * (ix + 1) / cols,
                r.y0 + r.height * (iy + 1) / rows,
            )
            pm = page.get_pixmap(dpi=dpi, clip=clip)
            pm.save(str(OUT / ('%s_r%dc%d.png' % (tag, iy + 1, ix + 1))))
            print('tile', tag, iy + 1, ix + 1, pm.width, pm.height)
