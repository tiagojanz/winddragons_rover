/* WindDragons Rover - Shared Client Logic */
function showToast(msg, isError = false) {
  let t = document.getElementById('toast');
  if (!t) {
    t = document.createElement('div');
    t.id = 'toast';
    document.body.appendChild(t);
  }
  t.innerText = msg;
  t.style.background = isError ? '#ef4444' : '#10b981';
  t.style.color = '#fff';
  t.style.display = 'block';
  setTimeout(() => { t.style.display = 'none'; }, 3500);
}

function formatBytes(bytes) {
  if (bytes === 0) return '0 B';
  const k = 1024;
  const sizes = ['B', 'KB', 'MB', 'GB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}

// Background status poller for updating navbar badges
function pollGlobalStatus() {
  fetch('/api/status')
    .then(r => r.json())
    .then(data => {
      const badge = document.getElementById('navWifiBadge');
      if (badge && data.wifi) {
        if (data.wifi.is_ap) {
          badge.className = 'badge badge-ap';
          badge.innerHTML = '<i class="fa-solid fa-tower-broadcast"></i> AP: ' + data.wifi.ssid;
        } else {
          badge.className = 'badge badge-sta';
          badge.innerHTML = '<i class="fa-solid fa-wifi"></i> ' + data.wifi.ip + ' (' + data.wifi.rssi + ' dBm)';
        }
      }
      const stationSpan = document.getElementById('navStationId');
      if (stationSpan && (data.station_id || data.mac_address)) {
        stationSpan.innerText = data.station_id || data.mac_address;
      }
      const baseMacEl = document.getElementById('baseMacVal');
      if (baseMacEl && (data.station_id || data.mac_address)) {
        baseMacEl.innerText = data.station_id || data.mac_address;
      }
      const baseNameEl = document.getElementById('baseStationName');
      if (baseNameEl && data.station_name) {
        baseNameEl.innerText = data.station_name;
      }
      const roverSpan = document.getElementById('navRoverId');
      if (roverSpan && data.rover_id) {
        roverSpan.innerText = 'Rover-' + data.rover_id;
      }
      const opBadge = document.getElementById('navOpStateBadge');
      const opText = document.getElementById('navOpStateText');
      if (opText && data.operation_state) {
        opText.innerText = data.operation_state;
        const colors = {
          'STORAGE': '#64748b',
          'WAITING_ROVERS': '#f59e0b',
          'WAITING ROVERS': '#f59e0b',
          'IDLE': '#06b6d4',
          'GO': '#10b981'
        };
        if (opBadge) {
          opBadge.style.background = colors[data.operation_state] || '#64748b';
        }
      }
      if (data.version) {
        const laf = data.version.lafvin || 'v2.1.0';
        const c6 = data.version.c6 || '--';
        const bun = data.version.bundle || 'v2.1.0';

        const navLaf = document.getElementById('navLafvinVer');
        if (navLaf) navLaf.innerText = 'Lafvin ' + laf;
        const navC6 = document.getElementById('navC6Ver');
        if (navC6) {
          navC6.innerText = 'C6 ' + c6;
          navC6.style.color = (c6 !== '--' && !c6.includes('detetar')) ? 'var(--yellow)' : 'var(--muted)';
        }

        const footLaf = document.getElementById('footerLafvinVer');
        if (footLaf) footLaf.innerText = laf;
        const footC6 = document.getElementById('footerC6Ver');
        if (footC6) footC6.innerText = c6;
        const footBun = document.getElementById('footerBundleVer');
        if (footBun) footBun.innerText = bun;

        const mainLaf = document.getElementById('mainLafvinVer');
        if (mainLaf) mainLaf.innerText = laf;
        const mainC6 = document.getElementById('mainC6Ver');
        if (mainC6) {
          mainC6.innerText = c6;
          mainC6.style.color = (c6 !== '--' && !c6.includes('detetar')) ? 'var(--yellow)' : 'var(--muted)';
        }

        const cfgLaf = document.getElementById('fwLafvinVer');
        if (cfgLaf) cfgLaf.innerText = laf;
        const cfgC6 = document.getElementById('fwC6Ver');
        if (cfgC6) {
          cfgC6.innerText = c6;
          cfgC6.style.color = (c6 !== '--' && !c6.includes('detetar')) ? 'var(--yellow)' : 'var(--muted)';
        }
      }
    })
    .catch(() => {});
}

document.addEventListener('DOMContentLoaded', () => {
  pollGlobalStatus();
  setInterval(pollGlobalStatus, 3000);
});
