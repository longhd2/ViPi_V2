function formatUptime(sec){
  sec = Number(sec || 0);

  const d = Math.floor(sec / 86400);
  const h = Math.floor((sec % 86400) / 3600);
  const m = Math.floor((sec % 3600) / 60);

  if(d > 0) return `${d} ngày ${h} giờ`;
  if(h > 0) return `${h} giờ ${m} phút`;

  return `${m} phút`;
}


async function json(url,options){
  const r=await fetch(url,options);
  return await r.json();
}

async function refresh(){
  try{
    const d=await json("/api/dashboard");

    internet.textContent=d.internet.online?"ONLINE":"OFFLINE";
    internet.className=d.internet.online?"online":"offline";
    latency.textContent=d.internet.online
      ? d.internet.latency_ms+" ms"
      : "Không có Internet";

    cpu.textContent=d.system.cpu+"%";
    temp.textContent=d.system.temperature==null
      ? "Không có cảm biến"
      : d.system.temperature+" °C";

    ram.textContent=d.system.ram_percent+"%";
    ramText.textContent=d.system.ram_used_gb+" / "+d.system.ram_total_gb+" GB";

    disk.textContent=d.system.disk_percent+"%";
    diskText.textContent=d.system.disk_used_gb+" / "+d.system.disk_total_gb+" GB";

    hostname.textContent=d.system.hostname;
    lanIp.textContent=d.system.lan_ip;
    document.getElementById("uptime").textContent=formatUptime(d.system.uptime);
    cores.textContent=d.system.cpu_count;
    load.textContent=d.system.load.join(" / ");

    supervisorStatus.textContent=d.supervisor.output || "N/A";

    if(d.home_assistant){
      const ha = d.home_assistant;

      const dot =
        document.getElementById("haServiceDot");

      const text =
        document.getElementById("haServiceText");

      if(dot && text){

        if(ha.connected){
          dot.className = "dot green";

          const source =
            ha.source === "internal"
              ? "LAN"
              : ha.source === "external"
                ? "External"
                : "";

          text.textContent =
            "Connected"
            +
            (source ? " · " + source : "")
            +
            (
              ha.latency_ms != null
                ? " · " + ha.latency_ms + "ms"
                : ""
            );

        }else if(ha.configured){
          dot.className = "dot gray";

          text.textContent =
            ha.message || "Disconnected";

        }else{
          dot.className = "dot gray";
          text.textContent = "Chưa cấu hình";
        }
      }
    }

    const online=d.clients.filter(x=>x.online).length;
    clientCount.textContent=online+" online";

    if(!d.clients.length){
      clients.innerHTML='<div class="empty">Chưa có dữ liệu Client heartbeat</div>';
    }else{
      clients.innerHTML=d.clients.map(c=>`
        <div class="client">

          <div>
            <div class="client-name">
              ${c.device_id || "unknown"}
            </div>

            <small class="${c.online ? 'online' : 'offline'}">
              ● ${c.online ? 'Online' : 'Offline'}
              ${c.age != null ? ' · ' + c.age + ' giây trước' : ''}
            </small>
          </div>

          <div>
            <small>IP</small><br>
            <b>${c.ip || "--"}</b>
          </div>

          <div>
            <small>ROOM</small><br>
            <b>${c.room || "default"}</b>
          </div>

          <div>
            <small>MODE</small><br>
            <b>${c.mode || "--"}</b>
          </div>

          <div>
            <small>VOLUME</small><br>
            <b>${c.volume != null ? c.volume + '%' : '--'}</b>
          </div>

          ${c.firmware ? `
          <div>
            <small>FIRMWARE</small><br>
            <b>${c.firmware}</b>
          </div>` : ''}

          ${c.build_id ? `
          <div>
            <small>BUILD</small><br>
            <b>${c.build_id}</b>
          </div>` : ''}

        </div>
      `).join("");
    }
  }catch(e){
    serverBadge.textContent="● WEB ERROR";
    serverBadge.className="badge offline";
  }
}

async function loadLogs(){
  try{
    const service=document.getElementById("logService").value;
    const d=await json("/api/logs?service="+encodeURIComponent(service));
    logs.textContent=d.log||"Chưa có log.";
    logs.scrollTop=logs.scrollHeight;
  }catch(e){
    logs.textContent="Không đọc được log.";
  }
}

async function control(service,action){
  if(action==="restart" &&
     !confirm("Khởi động lại "+service+"?")) return;

  if(action==="stop" &&
     !confirm("Dừng "+service+"?")) return;

  const d=await json("/api/control",{
    method:"POST",
    headers:{"Content-Type":"application/json"},
    body:JSON.stringify({service,action})
  });

  alert(d.output||"Đã gửi lệnh");
  setTimeout(refresh,800);
}

refresh();
loadLogs();
setInterval(refresh,3000);
setInterval(loadLogs,5000);
