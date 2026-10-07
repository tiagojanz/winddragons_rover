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
      const roverSpan = document.getElementById('navRoverId');
      if (roverSpan && data.rover_id) {
        roverSpan.innerText = 'Rover-' + data.rover_id;
      }
    })
    .catch(() => {});
}

document.addEventListener('DOMContentLoaded', () => {
  pollGlobalStatus();
  setInterval(pollGlobalStatus, 3000);
});
