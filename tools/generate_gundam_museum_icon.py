#!/usr/bin/env python3
"""Build the museum's native geometric helmet icon in the existing RGB565 format."""
from pathlib import Path
from PIL import Image, ImageDraw

root=Path(__file__).resolve().parents[1]
im=Image.new('RGB',(200,200),(0,0,0))
d=ImageDraw.Draw(im)
white=(228,234,230);shade=(140,158,163);dark=(26,38,48);red=(193,47,45);gold=(244,199,67)
d.polygon([(55,90),(66,58),(90,47),(114,49),(139,70),(150,143),(123,168),(76,168),(51,143)],fill=white)
d.polygon([(52,107),(67,115),(74,154),(56,140)],fill=shade)
d.polygon([(133,113),(148,105),(150,143),(130,157)],fill=shade)
d.polygon([(62,91),(137,91),(131,116),(70,116)],fill=dark)
d.polygon([(69,97),(94,101),(82,109)],fill=gold)
d.polygon([(106,101),(131,97),(118,109)],fill=gold)
d.polygon([(99,109),(117,123),(113,145),(100,151),(84,143),(81,124)],fill=white)
d.rectangle((91,125,94,138),fill=dark);d.rectangle((106,125,109,138),fill=dark)
d.polygon([(88,147),(112,147),(108,165),(97,170)],fill=red)
d.polygon([(94,75),(23,38),(46,75),(97,91)],fill=white)
d.polygon([(106,75),(177,38),(154,75),(103,91)],fill=white)
d.polygon([(91,68),(106,68),(109,91),(96,97),(90,86)],fill=red)
for x in (58,134):
    for y in (120,130,140):d.rectangle((x,y,x+9,y+4),fill=dark)
out=root/'main/assets/images'
im.save(out/'icon_gundam_museum.png')
values=[]
for r,g,b in im.getdata():
    p=((r&248)<<8)|((g&252)<<3)|(b>>3);values.extend((str(p&255),str(p>>8)))
rows=['    '+','.join(values[i:i+32])+',' for i in range(0,len(values),32)]
source='#include <lvgl.h>\n\nconst LV_ATTRIBUTE_MEM_ALIGN uint8_t icon_gundam_museum_map[] = {\n'+'\n'.join(rows)+'\n};\n'
source+='const lv_image_dsc_t icon_gundam_museum = {\n    .header.cf=LV_COLOR_FORMAT_RGB565,\n    .header.magic=LV_IMAGE_HEADER_MAGIC,\n    .header.w=200,\n    .header.h=200,\n    .data_size=80000,\n    .data=icon_gundam_museum_map,\n};\n'
(out/'icon_gundam_museum.c').write_text(source)
