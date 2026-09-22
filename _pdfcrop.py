import sys
import pathlib
import fitz  # PyMuPDF

OUT = pathlib.Path(r'd:\02_Project_Source\STM32_0914\my_project\_sch_png')
OUT.mkdir(exist_ok=True)

# 用法: python _pdfcrop.py <pdf> <tag> x0 y0 x1 y1 dpi   (坐标为 0~1 归一化)
args = sys.argv[1:]
src = args[0]
tag = args[1]
x0, y0, x1, y1, dpi = float(args[2]), float(args[3]), float(args[4]), float(args[5]), int(args[6])

doc = fitz.open(src)
page = doc[0]
r = page.rect
clip = fitz.Rect(
    r.x0 + r.width * x0,
    r.y0 + r.height * y0,
    r.x0 + r.width * x1,
    r.y0 + r.height * y1,
)
pm = page.get_pixmap(dpi=dpi, clip=clip)
out = OUT / (tag + '.png')
pm.save(str(out))
print('wrote', out.name, pm.width, pm.height)
