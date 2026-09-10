async function api(url, options){
  const r = await fetch(url, options);

  let data;

  try{
    data = await r.json();
  }catch{
    data = {};
  }

  if(!r.ok){
    throw new Error(
      data.detail ||
      `HTTP ${r.status}`
    );
  }

  return data;
}


function showTab(name, button){
  document
    .querySelectorAll(".cfg-panel")
    .forEach(x => x.classList.remove("active"));

  document
    .querySelectorAll(".cfg-tab")
    .forEach(x => x.classList.remove("active"));

  document
    .getElementById(`tab-${name}`)
    .classList.add("active");

  button.classList.add("active");
}


function weatherPayload(){
  return {
    location_name:
      document.getElementById("locationName").value.trim(),

    latitude:
      Number(document.getElementById("latitude").value),

    longitude:
      Number(document.getElementById("longitude").value),

    timezone:
      document.getElementById("timezone").value.trim(),

    forecast_days:
      Number(document.getElementById("forecastDays").value),

    rain_threshold:
      Number(document.getElementById("rainThreshold").value)
  };
}


async function loadConfig(){
  try{
    const d = await api("/api/config");
    const w = d.weather || {};

    locationName.value =
      w.location_name || "";

    latitude.value =
      w.latitude ?? "";

    longitude.value =
      w.longitude ?? "";

    timezone.value =
      w.timezone || "Asia/Ho_Chi_Minh";

    forecastDays.value =
      w.forecast_days ?? 7;

    rainThreshold.value =
      w.rain_threshold ?? 50;

  }catch(e){
    weatherTest.textContent =
      `Không đọc được Config: ${e.message}`;
  }
}


async function searchLocation(){
  const q = locationSearch.value.trim();

  if(q.length < 2){
    locationResults.innerHTML =
      "<div class='notice'>Nhập ít nhất 2 ký tự.</div>";

    return;
  }

  locationResults.innerHTML =
    "<div class='notice'>Đang tìm...</div>";

  try{
    const d = await api(
      `/api/geocode?q=${encodeURIComponent(q)}`
    );

    if(!d.results.length){
      locationResults.innerHTML =
        "<div class='notice'>Không tìm thấy địa điểm.</div>";

      return;
    }

    locationResults.innerHTML =
      d.results.map((x, i) => {

        const label = [
          x.name,
          x.admin2,
          x.admin1,
          x.country
        ].filter(Boolean).join(", ");

        return `
          <button class="location-item"
                  onclick='selectLocation(${JSON.stringify(x)})'>
            <b>${label}</b>
            <small>
              ${x.latitude}, ${x.longitude}
              · ${x.timezone || ""}
            </small>
          </button>
        `;
      }).join("");

  }catch(e){
    locationResults.innerHTML =
      `<div class="notice error">${e.message}</div>`;
  }
}


function selectLocation(x){
  locationName.value = x.name || "";
  latitude.value = x.latitude ?? "";
  longitude.value = x.longitude ?? "";
  timezone.value =
    x.timezone || "Asia/Ho_Chi_Minh";

  locationResults.innerHTML =
    `<div class="selected-location">
       Đã chọn:
       <b>${x.name}</b>
     </div>`;

  weatherState.textContent = "Unsaved";
}


async function testWeather(){
  weatherTest.textContent =
    "Đang kiểm tra Open-Meteo...";

  try{
    const d = await api(
      "/api/config/weather/test",
      {
        method:"POST",
        headers:{
          "Content-Type":"application/json"
        },
        body:JSON.stringify(weatherPayload())
      }
    );

    weatherTest.textContent =
`Địa điểm : ${d.location_name}
Hiện tại  : ${d.temperature ?? "--"} °C
Hôm nay   : ${d.min ?? "--"} → ${d.max ?? "--"} °C
Khả năng mưa: ${d.rain ?? "--"} %`;

  }catch(e){
    weatherTest.textContent =
      `Weather Test lỗi: ${e.message}`;
  }
}


async function saveWeather(){
  weatherState.textContent = "Saving...";

  try{
    await api(
      "/api/config/weather",
      {
        method:"PUT",
        headers:{
          "Content-Type":"application/json"
        },
        body:JSON.stringify(weatherPayload())
      }
    );

    weatherState.textContent = "Saved";

  }catch(e){
    weatherState.textContent = "Error";
    alert(e.message);
  }
}


document
  .getElementById("locationSearch")
  .addEventListener(
    "keydown",
    e => {
      if(e.key === "Enter"){
        searchLocation();
      }
    }
  );


loadConfig();


/* ===================================================
   HOME ASSISTANT
   =================================================== */

async function loadHaConfig(){
  try{
    const d =
      await api("/api/config/home-assistant");

    haEnabled.checked =
      !!d.enabled;

    haPreferInternal.checked =
      d.prefer_internal !== false;

    haInternalUrl.value =
      d.internal_url || "";

    haExternalUrl.value =
      d.external_url || "";

    haTimeout.value =
      d.timeout || 5;

    haToken.value = "";

    haTokenState.textContent =
      d.token_configured
        ? "● Token đã được cấu hình"
        : "○ Chưa có token";

    haTokenState.className =
      d.token_configured
        ? "token-ok"
        : "";

    haConfigState.textContent =
      d.enabled
        ? "Enabled"
        : "Disabled";

  }catch(e){
    haConfigState.textContent =
      "Config Error";
  }
}


function haPayload(){
  return {
    enabled:
      haEnabled.checked,

    prefer_internal:
      haPreferInternal.checked,

    internal_url:
      haInternalUrl.value.trim(),

    external_url:
      haExternalUrl.value.trim(),

    token:
      haToken.value.trim(),

    timeout:
      Number(haTimeout.value || 5)
  };
}


function toggleHaToken(){
  const input =
    document.getElementById("haToken");

  input.type =
    input.type === "password"
      ? "text"
      : "password";
}


function renderHaResult(d){
  const card =
    document.getElementById(
      "haConnectionCard"
    );

  const title =
    document.getElementById(
      "haConnectionTitle"
    );

  const text =
    document.getElementById(
      "haConnectionText"
    );

  if(d.ok){
    card.className =
      "ha-connection success";

    title.textContent =
      "Home Assistant Connected";

    const source =
      d.source === "internal"
        ? "LAN"
        : d.source === "external"
          ? "External"
          : "Auto";

    text.textContent =
      `${source} · ${d.url || ""}`
      +
      (
        d.latency_ms != null
          ? ` · ${d.latency_ms} ms`
          : ""
      )
      +
      (
        d.version
          ? ` · HA ${d.version}`
          : ""
      );

  }else{
    card.className =
      "ha-connection error";

    title.textContent =
      "Không kết nối được";

    text.textContent =
      d.message || "Connection failed";
  }
}


async function testHa(mode){
  const card =
    document.getElementById(
      "haConnectionCard"
    );

  card.className =
    "ha-connection testing";

  haConnectionTitle.textContent =
    "Đang kiểm tra...";

  haConnectionText.textContent =
    mode === "internal"
      ? "Đang kiểm tra địa chỉ LAN"
      : mode === "external"
        ? "Đang kiểm tra địa chỉ bên ngoài"
        : "Đang thử theo thứ tự ưu tiên";

  try{
    const payload = haPayload();
    payload.mode = mode;

    const d =
      await api(
        "/api/home-assistant/test",
        {
          method:"POST",

          headers:{
            "Content-Type":
              "application/json"
          },

          body:
            JSON.stringify(payload)
        }
      );

    renderHaResult(d);

  }catch(e){
    renderHaResult({
      ok:false,
      message:e.message
    });
  }
}


async function saveHa(){
  haConfigState.textContent =
    "Saving...";

  try{
    const payload = haPayload();

    await api(
      "/api/config/home-assistant",
      {
        method:"PUT",

        headers:{
          "Content-Type":
            "application/json"
        },

        body:
          JSON.stringify(payload)
      }
    );

    haToken.value = "";

    haConfigState.textContent =
      "Saved";

    await loadHaConfig();

    if(haEnabled.checked){
      await testHa("auto");
    }

  }catch(e){
    haConfigState.textContent =
      "Error";

    alert(e.message);
  }
}


loadHaConfig();
