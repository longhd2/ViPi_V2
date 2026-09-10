import asyncio,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from main import build_app
from core.context import ClientContext
from understanding.engine import UnderstandingEngine

ctx=ClientContext('dev1','bedroom')
eng=UnderstandingEngine()
assert eng.match('mở máy lạnh',ctx).name=='home.control'
assert eng.match('phòng nóng quá bật máy lạnh',ctx).slots['room']=='bedroom'
assert eng.match('tăng âm lượng',ctx).name=='volume.up'
assert eng.match('hẹn giờ 5 phút',ctx).slots['seconds']==300

async def main():
 app=build_app()
 r=await app.handle_text('tăng âm lượng',ctx); assert r['data']['command']=='volume_up'
 r=await app.handle_text('bật đèn',ctx); assert 'Home Assistant' in r['reply']
 r=await app.handle_text('dừng nhạc',ctx); assert r['data']['command']=='media_stop'
 r=await app.handle_text('hẹn giờ 1 phút',ctx); assert r['data']['seconds']==60
 print('SERVER TESTS: PASS')
asyncio.run(main())
