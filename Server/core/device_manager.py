from .context import ClientContext

class DeviceManager:
    def __init__(self): self._devices = {}
    def upsert(self, ctx: ClientContext): self._devices[ctx.device_id] = ctx
    def get(self, device_id: str): return self._devices.get(device_id)
    def all(self): return list(self._devices.values())
    def set_online(self, device_id: str, online: bool):
        if device_id in self._devices: self._devices[device_id].online = online
