"""Render original controller symbols for the in-game menu. No game assets."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root=Path(__file__).resolve().parents[1]
output=root/'scripts/BullyMotion/ui';output.mkdir(exist_ok=True)
size=256;ink=(241,231,202,255);dark=(19,27,39,255)
font_path=Path('C:/Windows/Fonts/arialbd.ttf')
font=ImageFont.truetype(str(font_path) if font_path.exists() else 'DejaVuSans-Bold.ttf',90)
for name in ('right-stick','dpad-left','dpad-down','dpad-right'):
    image=Image.new('RGBA',(size,size));draw=ImageDraw.Draw(image)
    if name=='right-stick':
        draw.ellipse((19,28,237,246),fill=dark,outline=ink,width=13)
        draw.text((128,151),'R',font=font,fill=ink,anchor='mm')
        draw.polygon([(114,30),(114,5),(142,5),(142,30),(163,30),(128,66),(93,30)],fill=ink)
    else:
        draw.polygon([(91,16),(165,16),(165,91),(240,91),(240,165),(165,165),(165,240),(91,240),(91,165),(16,165),(16,91),(91,91)],fill=dark,outline=ink,width=9)
        if name=='dpad-left':draw.polygon([(81,100),(46,128),(81,156),(81,141),(112,141),(112,115),(81,115)],fill=ink)
        elif name=='dpad-down':draw.polygon([(100,174),(128,209),(156,174),(141,174),(141,143),(115,143),(115,174)],fill=ink)
        else:draw.polygon([(175,100),(210,128),(175,156),(175,141),(144,141),(144,115),(175,115)],fill=ink)
    image.resize((64,64),Image.Resampling.LANCZOS).save(output/(name+'.png'))
print('Original controller icons ready')
