from dataclasses import dataclass, field
from typing import Any

@dataclass
class ClientContext:
    device_id: str
    room: str = "default"
    volume: int = 70
    online: bool = True
    metadata: dict[str, Any] = field(default_factory=dict)
