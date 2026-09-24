if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", startApp);
} else {
    startApp();
}

function startApp() {
    injectCommonLayout();
    fetchHardwareDiagnostics();
    
    if (document.getElementById("configForm")) loadConfigValues();
    if (document.getElementById("fileListContainer")) loadAudioDirectory();
    setupConfigFormHandler();
}

function injectCommonLayout() {
    const currentPath = window.location.pathname;
    const isActive = (path) => (currentPath === path || currentPath.endsWith(path) || (currentPath === '/' && path === '/index.html')) ? 'active' : '';

    const header = document.querySelector('header');
    if (header) {
        header.innerHTML = `
            <button class="navbar-toggler" type="button">
                <span></span>
                <span></span>
                <span></span>
            </button>
            <a class="navbar-brand fw-bold text-white" href="/">
                <img src="/logo.png" alt="Logo">
                <span><span class="text-primary-accent">Garmin LidarLite V4</span> Test</span>
            </a>`;
    }

    const sidebar = document.getElementById('sidebarMenu');
    if (sidebar) {
        sidebar.innerHTML = `
            <div class="sidebar-content-wrapper">
                <div>
                    <div class="offcanvas-header">
                        <h5 class="fw-bold text-white sidebar-title">
                            <img src="/logo.png" alt="Logo" style="height:24px; width:auto; object-fit:contain;">
                            <span><span class="text-primary-accent">Garmin LidarLite V4</span> Test</span>
                        </h5>
                        <button type="button" id="closeSidebarBtn" class="d-md-none close-sidebar-btn">&times;</button>
                    </div>
                    <ul class="sidebar-nav">
                        <li class="nav-item"><a class="nav-link ${isActive('/index.html')}" href="/index.html"><img src="/icons/microchip.svg" class="nav-link-icon"><span>System</span></a></li>
                        <li class="nav-item"><a class="nav-link ${isActive('/settings.html')}" href="/settings.html"><img src="/icons/setting.svg" class="nav-link-icon"><span>Settings</span></a></li>
                        <li class="nav-item"><a class="nav-link ${isActive('/console.html')}" href="/console.html"><img src="/icons/terminal.svg" class="nav-link-icon"><span>Console</span></a></li>
                    </ul>
                </div>
                <div class="sidebar-footer">
                    <div id="statusBadge" class="status-badge status-loading w-100"><span class="status-dot"></span><span id="statusText">Initializing...</span></div>
                </div>
            </div>`;
    }

    const footer = document.querySelector('footer');
    if (footer) {
        footer.innerHTML = `Copyright &copy; Real Time Solutions Pvt. Ltd.`;
    }
    initLayoutNavigation();
}

function initLayoutNavigation() {
    const toggleBtn = document.querySelector(".navbar-toggler");
    const closeBtn = document.getElementById("closeSidebarBtn");
    const sidebar = document.getElementById("sidebarMenu");

    if (toggleBtn && sidebar) {
        toggleBtn.addEventListener("click", (e) => {
            e.stopPropagation();
            sidebar.classList.toggle("show");
        });
    }

    if (closeBtn && sidebar) {
        closeBtn.addEventListener("click", () => sidebar.classList.remove("show"));
    }

    document.addEventListener("click", (e) => {
        if (sidebar && sidebar.classList.contains("show") && !sidebar.contains(e.target) && (!toggleBtn || !toggleBtn.contains(e.target))) {
            sidebar.classList.remove("show");
        }
    });

    const togglePassBtn = document.getElementById('togglePassBtn');
    if (togglePassBtn) {
        togglePassBtn.addEventListener('click', function () {
            const passInput = document.getElementById('ftp_pass');
            const iconImg = document.getElementById('togglePassIcon');
            if (!passInput || !iconImg) return;
        
            const isPassword = passInput.getAttribute('type') === 'password';
            passInput.setAttribute('type', isPassword ? 'text' : 'password');
        
            if (isPassword) {
                iconImg.src = "/icons/eye_slash.svg";
                iconImg.alt = "Hide Password";
            } else {
                iconImg.src = "/icons/eye.svg";
                iconImg.alt = "Show Password";
            }
        });
    }
}

function setStatus(state, message) {
    const badge = document.getElementById("statusBadge");
    const text = document.getElementById("statusText");
    if (!badge || !text) return;

    badge.className = "status-badge " + state;
    text.textContent = message;
}

function showUINotification(message, isError = false) {
    const alertBox = document.getElementById("alertContainer");
    if (!alertBox) return;

    alertBox.innerHTML = `
        <div style="padding:1rem; margin-bottom:1.5rem; border-radius:0.5rem; font-weight:600; font-size:0.9rem;
                    background-color:${isError ? '#f8d7da' : '#d1e7dd'}; 
                    color:${isError ? '#842029' : '#0f5132'}; 
                    border:1px solid ${isError ? '#f5c2c7' : '#badbcc'};">
            ${message}
        </div>`;
    
    setTimeout(() => { alertBox.innerHTML = ""; }, 5000);
}

function fetchHardwareDiagnostics() {
    setStatus("status-loading", "Refreshing...");
    fetch("/api/config")
        .then(res => {
            if (!res.ok) throw new Error(`HTTP status ${res.status}`);
            const contentType = res.headers.get("content-type");
            if (!contentType || !contentType.includes("application/json")) {
                throw new TypeError("Expected JSON but received HTML. Check server API routes.");
            }
            return res.json();
        })
        .then(data => {
            setStatus("status-connected", "Connected");
            if (document.getElementById("info_product")) document.getElementById("info_product").textContent = data.product;
            if (document.getElementById("info_mac")) document.getElementById("info_mac").textContent = data.mac;
            if (document.getElementById("info_ip")) document.getElementById("info_ip").textContent = data.ip;
            if (document.getElementById("info_firmware")) document.getElementById("info_firmware").textContent = data.firmware;
            if (document.getElementById("info_heap")) document.getElementById("info_heap").textContent = (data.heap / 1024).toFixed(1) + " KB";
        })
        .catch(() => setStatus("status-disconnected", "Disconnected"));
}

function loadConfigValues() {
    fetch("/api/config")
        .then(res => {
            if (!res.ok) throw new Error(`HTTP status ${res.status}`);
            const contentType = res.headers.get("content-type");
            if (!contentType || !contentType.includes("application/json")) {
                throw new TypeError("Expected JSON but received HTML.");
            }
            return res.json();
        })
        .then(data => {
            if (document.getElementById("interval")) document.getElementById("interval").value = data.schedule_interval ?? 10;
            if (document.getElementById("duration")) document.getElementById("duration").value = data.rec_duration ?? 10;
            if (document.getElementById("unit")) document.getElementById("unit").value = ["min", "hour", "day", "week"][data.schedule_mode] || "min";
            
            if (document.getElementById("startDateTime")) {
                const year = data.start_year || 2026;
                const month = String(data.start_month || 1).padStart(2, '0');
                const day = String(data.start_day || 1).padStart(2, '0');
                const hour = String(data.start_hour ?? 0).padStart(2, '0');
                const min = String(data.start_min ?? 0).padStart(2, '0');
                
                document.getElementById("startDateTime").value = `${year}-${month}-${day}T${hour}:${min}`;
            }

            if (document.getElementById("ftp_host")) document.getElementById("ftp_host").value = data.ftp_host || "";
            if (document.getElementById("ftp_port")) document.getElementById("ftp_port").value = data.ftp_port || 21;
            if (document.getElementById("ftp_user")) document.getElementById("ftp_user").value = data.ftp_user || "";
            if (document.getElementById("ftp_pass")) document.getElementById("ftp_pass").value = data.ftp_pass || "";
        })
        .catch(err => {
            console.warn("Configuration could not be loaded:", err.message);
        });
}

function setupConfigFormHandler() {
    const configForm = document.getElementById("configForm");
    if (configForm) {
        configForm.addEventListener("submit", (e) => {
            e.preventDefault();

            if (!configForm.checkValidity()) {
                configForm.reportValidity();
                return;
            }

            const payload = new URLSearchParams();
            
            if (document.getElementById("interval")) {
                const intervalVal = parseInt(document.getElementById("interval").value) || 0;
                const durationVal = parseInt(document.getElementById("duration").value) || 0;
                const unitValue = document.getElementById("unit")?.value || "min";

                if (unitValue === "min" && intervalVal < 1) {
                    showUINotification("Threshold check failed: The minimum interval configuration allowed is 1 minutes.", true);
                    return;
                }
                if (durationVal < 10) {
                    showUINotification("Threshold check failed: The minimum audio recording duration allowed is 10 seconds.", true);
                    return;
                }

                const units = ["min", "hour", "day", "week"];
                const modeIndex = units.indexOf(unitValue) !== -1 ? units.indexOf(unitValue) : 0;

                let year = 2026, month = 1, day = 1, hour = 0, min = 0;
                const dateTimeVal = document.getElementById("startDateTime")?.value;
                
                if (dateTimeVal) {
                    const [datePart, timePart] = dateTimeVal.split("T");
                    if (datePart) {
                        const datePieces = datePart.split("-");
                        year = parseInt(datePieces[0]) || 2026;
                        month = parseInt(datePieces[1]) || 1;
                        day = parseInt(datePieces[2]) || 1;
                    }
                    if (timePart) {
                        const timePieces = timePart.split(":");
                        hour = parseInt(timePieces[0]) || 0;
                        min = parseInt(timePieces[1]) || 0;
                    }
                }

                payload.append("page_scope", "schedule");
                payload.append("schedule_mode", modeIndex);
                payload.append("schedule_interval", intervalVal);
                payload.append("start_year", year);
                payload.append("start_month", month);
                payload.append("start_day", day);
                payload.append("start_hour", hour);
                payload.append("start_min", min);
                payload.append("rec_duration", durationVal);
                
            } else if (document.getElementById("ftp_host")) {
                payload.append("page_scope", "ftp");
                payload.append("ftp_host", document.getElementById("ftp_host").value);
                payload.append("ftp_port", document.getElementById("ftp_port").value);
                payload.append("ftp_user", document.getElementById("ftp_user").value);
                payload.append("ftp_pass", document.getElementById("ftp_pass").value);
            }

            setStatus("status-loading", "Saving...");
            fetch("/api/post-config", { 
                method: "POST", 
                body: payload.toString(), 
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' } 
            })
            .then(res => res.text())
            .then(res => { 
                if (res === "SUCCESS") {
                    fetchHardwareDiagnostics();
                    showUINotification("Configuration profiles applied successfully.");
                } else {
                    showUINotification("Failed to apply requested system change parameters.", true);
                }
            })
            .catch(() => {
                setStatus("status-disconnected", "Save Interrupted");
                showUINotification("Network connectivity dropped. Sync aborted.", true);
            });
        });
    }
}

function triggerHardwareRestart() {
    if (!confirm("Are you sure you want to reboot the system gateway firmware?")) return;
    
    setStatus("status-loading", "Rebooting...");
    fetch("/api/restart", { method: "POST" })
        .then(res => res.text())
        .then(text => {
            alert("System is rebooting. Please wait a moment before refreshing.");
            location.reload();
        })
        .catch(() => {
            alert("Restart command sent. The connection will close while hardware recycles.");
        });
}
