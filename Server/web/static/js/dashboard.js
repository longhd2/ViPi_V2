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

    const sv=document.getElementById("serverVersion");
    const cv=document.getElementById("clientLatest");

    if(sv){
      sv.textContent=d.version?.server || "--";
    }

    if(cv){
      cv.textContent=
        d.version?.client_latest?.ok
          ? d.version.client_latest.version
          : "Không kiểm tra được";
    }

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

    // Lay firmware/build truc tiep tu /status cua tung Client
    await Promise.all(d.clients.map(async c => {
      if (!c.online || !c.ip) return;
      try {
        const r = await fetch(
          "/api/client/status?ip=" + encodeURIComponent(c.ip),
          {cache:"no-store"}
        );
        const x = await r.json();
        if (x?.status?.ok) c._status = x.status;
      } catch(e) {}
    }));

    const online=d.clients.filter(x=>x.online).length;
    clientCount.textContent=online+" online";

    if(!d.clients.length){
      clients.innerHTML='<div class="empty">Chưa có dữ liệu Client heartbeat</div>';
    }else{
      clients.innerHTML=d.clients.map(c=>{
        const latest =
          d.version?.client_latest?.version || "--";

        const installed =
          c._status?.firmware || c.firmware || "--";

        const ip = c.ip || "";

        return `
        <div class="client client-device">

          <div class="client-head">
            <div>
              <div class="client-name">
                ${esc(c.device_id || "unknown")}
              </div>
              <small class="${c.online ? 'online' : 'offline'}">
                ● ${c.online ? 'Online' : 'Offline'}
                ${c.age != null ? ' · ' + c.age + ' giây trước' : ''}
              </small>
            </div>

            <span class="client-room">
              ${esc(c.room || "default")}
            </span>
          </div>

          <div>
            <small>IP</small><br>
            <b>${esc(ip || "--")}</b>
          </div>

          <div>
            <small>VOLUME</small><br>
            <b>${c.volume != null ? c.volume + '%' : '--'}</b>
          </div>

          <div>
            <small>INSTALLED</small><br>
            <b>${esc(installed)}</b>
          </div>

          <div>
            <small>LATEST</small><br>
            <b>${esc(latest)}</b>
          </div>

          <div class="client-build">
            <small>BUILD</small><br>
            <b>${esc(c._status?.build_id || c._status?.build || c.build_id || "--")}</b>
          </div>

          <div class="client-actions">
            <button
              onclick="openClientConfig('${esc(ip)}')">
              Cấu hình
            </button>

            <button
              class="client-check-update"
              data-ip="${esc(ip)}"
              onclick="checkClientUpdate('${esc(ip)}',this)">
              Kiểm tra
            </button>

            <button
              class="primary"
              onclick="otaClient('${esc(ip)}','${esc(latest)}')">
              Update OTA
            </button>
          </div>

          <div
            class="update-state"
            data-update-ip="${esc(ip)}">
            Chưa kiểm tra
          </div>

        </div>`;
      }).join("");
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


/* ===== ViPi Firmware / OTA ===== */

function esc(v){
  return String(v ?? "")
    .replaceAll("&","&amp;")
    .replaceAll("<","&lt;")
    .replaceAll(">","&gt;")
    .replaceAll('"',"&quot;");
}

async function getClientUpdate(ip){
  return await json(
    "/api/client/status?ip=" +
    encodeURIComponent(ip)
  );
}

async function checkClientUpdate(ip,button){
  if(!ip) return;

  const old = button ? button.textContent : "";

  if(button){
    button.disabled=true;
    button.textContent="Đang kiểm tra...";
  }

  try{
    const d=await getClientUpdate(ip);

    const box=document.querySelector(
      '[data-update-ip="'+CSS.escape(ip)+'"]'
    );

    if(box){
      if(!d.status?.ok){
        box.textContent="Không đọc được /status";
        box.className="update-state offline";
      }else if(d.update_available){
        box.textContent=
          "Có bản mới " +
          (d.latest?.version || "");
        box.className="update-state update-new";
      }else{
        box.textContent="Đã mới nhất";
        box.className="update-state online";
      }
    }
  }catch(e){
    alert("Không kiểm tra được Client: "+e);
  }finally{
    if(button){
      button.disabled=false;
      button.textContent=old;
    }
  }
}

async function otaClient(ip,version){
  if(!ip) return;

  if(!confirm(
    "Cập nhật ESP32 "+ip+
    " lên "+version+"?\n\n"+
    "Không tắt nguồn trong khi OTA."
  )) return;

  const box=document.querySelector(
    '[data-update-ip="'+CSS.escape(ip)+'"]'
  );

  if(box){
    box.textContent="Đang tải và cập nhật OTA...";
    box.className="update-state updating";
  }

  try{
    const d=await json("/api/client/ota",{
      method:"POST",
      headers:{"Content-Type":"application/json"},
      body:JSON.stringify({ip})
    });

    if(!d.ok){
      throw new Error(d.error || "OTA thất bại");
    }

    if(box){
      box.textContent=
        "OTA "+d.version+
        " hoàn tất · đang reboot";
      box.className="update-state online";
    }

    setTimeout(refresh,8000);
  }catch(e){
    if(box){
      box.textContent="OTA lỗi";
      box.className="update-state offline";
    }
    alert(e.message || e);
  }
}

async function openClientConfig(ip){
  if(!ip) return;
  window.open(
    "http://"+ip+"/",
    "_blank",
    "noopener"
  );
}

async function checkAllUpdates(){
  const buttons=[
    ...document.querySelectorAll(
      ".client-check-update"
    )
  ];

  for(const b of buttons){
    await checkClientUpdate(
      b.dataset.ip,
      b
    );
  }
}
