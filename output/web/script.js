// API Base URL
const API_BASE_URL = '';

// Current language
let currentLang = localStorage.getItem('lang') || 'en';

// Translations
const translations = {
    en: {
        // Navigation
        mediaServer: 'Media Server',
        navDevice: 'Device',
        navFile: 'File',
        navGbDomain: 'GB Domain',
        navWebrtc: 'WebRTC',
        navRtmp: 'RTMP',

        // Device section
        deviceList: 'Device List',
        addDevice: 'Add Device',
        refresh: 'Refresh',
        noDevicesFound: 'No devices found',
        ptzControl: 'PTZ Control',
        noPTZDevicesFound: 'No PTZ-capable devices found',
        zoomIn: 'Zoom +',
        zoomOut: 'Zoom -',

        // File section
        fileList: 'File List',
        upload: 'Upload',
        noFilesFound: 'No files found',

        // GB Domain section
        domainList: 'Domain List',
        gbRecord: 'GB Record',
        catalog: 'Catalog',
        serverInfo: 'Server Info',
        noGbDomainsFound: 'No GB domains found',

        // GB Record
        gbDevice: 'GB Device',
        selectDevice: '-- Select Device --',
        startTime: 'Start Time',
        endTime: 'End Time',
        type: 'Type',
        typeAll: 'All',
        typeTime: 'Time',
        typeAlarm: 'Alarm',
        typeManual: 'Manual',
        search: 'Search',
        searchRecordsHint: 'Select a device and time range to search records',
        noRecordsFound: 'No records found for the selected time range',

        // WebRTC section
        webrtcSessionList: 'WebRTC Session List',
        noSessionsFound: 'No WebRTC sessions found',

        // RTMP section
        rtmpStreamList: 'RTMP Stream List',
        noRtmpStreamsFound: 'No RTMP streams found',

        // Add Device Modal
        protocol: 'Protocol',
        name: 'Name',
        ipAddress: 'IP Address',
        username: 'Username',
        password: 'Password',
        enterDeviceName: 'Enter device name',
        enterPassword: 'Enter password',
        cancel: 'Cancel',
        add: 'Add',

        // Table headers
        deviceId: 'Device ID',
        status: 'Status',
        codec: 'Codec',
        resolution: 'Resolution',
        action: 'Action',
        fileId: 'File ID',
        fileName: 'File Name',
        size: 'Size',
        duration: 'Duration',
        frameRate: 'Frame Rate',
        domainId: 'Domain ID',
        deviceCount: 'Device Count',
        ip: 'IP',
        port: 'Port',
        sessionId: 'Session ID',
        videoCodec: 'Video Codec',
        audioCodec: 'Audio Codec',

        // Messages
        confirmDeleteDevice: 'Are you sure you want to delete',
        confirmDeleteFile: 'Are you sure you want to delete',
        deviceDeletedSuccess: 'Device deleted successfully',
        fileDeletedSuccess: 'File deleted successfully',
        deviceAddedSuccess: 'Device added successfully',
        uploadSuccess: 'Upload successful',
        uploadFailed: 'Upload failed',
        deleteFailed: 'Delete failed',
        addFailed: 'Failed to add device',
        nameRequired: 'Name is required',
        urlRequired: 'URL is required',
        ipRequired: 'IP Address is required',
        usernameRequired: 'Username is required',
        passwordRequired: 'Password is required',
        selectDeviceAlert: 'Please select a device',
        selectStartTime: 'Please select start time',
        selectEndTime: 'Please select end time',
        endTimeAfterStart: 'End time must be after start time',
        timeRangeLimit: 'Time range must be less than or equal to 1 hour',
        catalogSyncSuccess: 'Catalog sync initiated successfully',
        catalogSyncFailed: 'Catalog sync failed',
        failedToGetDeviceUrl: 'Failed to get device URL',
        failedToGetFileUrl: 'Failed to get file URL',
        failedToGetRecordUrl: 'Failed to get record URL',
        failedToGetServerInfo: 'Failed to get server info',
        browserNotSupported: 'Your browser does not support mpegts.js',

        // GB Server Info
        gbServerInfo: 'GB Server Information',
        rtpTransport: 'RTP Transport',

        // Record types display
        recordTypeTime: 'Time (Scheduled)',
        recordTypeAlarm: 'Alarm',
        recordTypeManual: 'Manual',

        // Uploading
        uploading: 'Uploading'
    },
    zh: {
        // Navigation
        mediaServer: '媒体服务器',
        navDevice: '设备',
        navFile: '文件',
        navGbDomain: '国标域',
        navWebrtc: 'WebRTC',

        // Device section
        deviceList: '设备列表',
        addDevice: '添加设备',
        refresh: '刷新',
        noDevicesFound: '暂无设备',
        ptzControl: '云台控制',
        noPTZDevicesFound: '暂无云台设备',
        zoomIn: '放大 +',
        zoomOut: '缩小 -',

        // File section
        fileList: '文件列表',
        upload: '上传',
        noFilesFound: '暂无文件',

        // GB Domain section
        domainList: '域列表',
        gbRecord: '录像查询',
        catalog: '目录同步',
        serverInfo: '服务器信息',
        noGbDomainsFound: '暂无国标域',

        // GB Record
        gbDevice: '国标设备',
        selectDevice: '-- 请选择设备 --',
        startTime: '开始时间',
        endTime: '结束时间',
        type: '类型',
        typeAll: '全部',
        typeTime: '定时',
        typeAlarm: '告警',
        typeManual: '手动',
        search: '搜索',
        searchRecordsHint: '请选择设备和时间范围搜索录像',
        noRecordsFound: '所选时间范围内未找到录像',

        // WebRTC section
        webrtcSessionList: 'WebRTC 会话列表',
        noSessionsFound: '暂无 WebRTC 会话',

        // RTMP section
        rtmpStreamList: 'RTMP流列表',
        noRtmpStreamsFound: '暂无RTMP流',

        // Add Device Modal
        protocol: '协议',
        name: '名称',
        ipAddress: 'IP 地址',
        username: '用户名',
        password: '密码',
        enterDeviceName: '请输入设备名称',
        enterPassword: '请输入密码',
        cancel: '取消',
        add: '添加',

        // Table headers
        deviceId: '设备ID',
        status: '状态',
        codec: '编码',
        resolution: '分辨率',
        action: '操作',
        fileId: '文件ID',
        fileName: '文件名',
        size: '大小',
        duration: '时长',
        frameRate: '帧率',
        domainId: '域ID',
        deviceCount: '设备数',
        ip: 'IP',
        port: '端口',
        sessionId: '会话ID',
        videoCodec: '视频编码',
        audioCodec: '音频编码',

        // Messages
        confirmDeleteDevice: '确定要删除',
        confirmDeleteFile: '确定要删除',
        deviceDeletedSuccess: '设备删除成功',
        fileDeletedSuccess: '文件删除成功',
        deviceAddedSuccess: '设备添加成功',
        uploadSuccess: '上传成功',
        uploadFailed: '上传失败',
        deleteFailed: '删除失败',
        addFailed: '添加设备失败',
        nameRequired: '请输入名称',
        urlRequired: '请输入URL',
        ipRequired: '请输入IP地址',
        usernameRequired: '请输入用户名',
        passwordRequired: '请输入密码',
        selectDeviceAlert: '请选择设备',
        selectStartTime: '请选择开始时间',
        selectEndTime: '请选择结束时间',
        endTimeAfterStart: '结束时间必须晚于开始时间',
        timeRangeLimit: '时间范围不能超过1小时',
        catalogSyncSuccess: '目录同步已启动',
        catalogSyncFailed: '目录同步失败',
        failedToGetDeviceUrl: '获取设备URL失败',
        failedToGetFileUrl: '获取文件URL失败',
        failedToGetRecordUrl: '获取录像URL失败',
        failedToGetServerInfo: '获取服务器信息失败',
        browserNotSupported: '您的浏览器不支持 mpegts.js',

        // GB Server Info
        gbServerInfo: '国标服务器信息',
        rtpTransport: 'RTP传输方式',

        // Record types display
        recordTypeTime: '定时录像',
        recordTypeAlarm: '告警录像',
        recordTypeManual: '手动录像',

        // Uploading
        uploading: '正在上传'
    }
};

// Get translation
function t(key) {
    return translations[currentLang][key] || translations['en'][key] || key;
}

// Escape HTML special characters
function escapeHtml(text) {
    if (text === null || text === undefined) return '';
    return String(text)
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;")
        .replace(/'/g, "&#039;");
}

// Apply translations to page
function applyTranslations() {
    // Update elements with data-i18n
    document.querySelectorAll('[data-i18n]').forEach(el => {
        const key = el.getAttribute('data-i18n');
        if (translations[currentLang][key]) {
            el.textContent = translations[currentLang][key];
        }
    });

    // Update placeholders
    document.querySelectorAll('[data-i18n-placeholder]').forEach(el => {
        const key = el.getAttribute('data-i18n-placeholder');
        if (translations[currentLang][key]) {
            el.placeholder = translations[currentLang][key];
        }
    });

    // Update page title
    document.title = t('mediaServer');
}

// Switch language
function switchLanguage(lang) {
    currentLang = lang;
    localStorage.setItem('lang', lang);

    // Update button states
    document.querySelectorAll('.lang-btn').forEach(btn => {
        btn.classList.toggle('active', btn.getAttribute('data-lang') === lang);
    });

    applyTranslations();

    // Reload current section data to update dynamic content
    const activeNav = document.querySelector('.nav-item.active');
    if (activeNav) {
        const target = activeNav.getAttribute('data-target');
        if (target === 'device') loadDevices();
        else if (target === 'file') loadFiles();
        else if (target === 'gbdomain') loadGbDomains();
        else if (target === 'webrtc') loadWebRTCSessions();
    }
}

// Language switcher event
document.querySelectorAll('.lang-btn').forEach(btn => {
    btn.addEventListener('click', function () {
        switchLanguage(this.getAttribute('data-lang'));
    });
});

// Protocol enum mapping
const PROTOCOL_MAP = {
    1: 'GB28181',
    2: 'RTSP',
    3: 'RTMP',
    4: 'ONVIF'
};

function getProtocolName(protocol) {
    return PROTOCOL_MAP[protocol] || '-';
}

// Navigation handling
document.querySelectorAll('.nav-item').forEach(item => {
    item.addEventListener('click', function () {
        // Update active nav item
        document.querySelectorAll('.nav-item').forEach(nav => nav.classList.remove('active'));
        this.classList.add('active');

        // Show corresponding section
        const target = this.getAttribute('data-target');
        document.querySelectorAll('.content-section').forEach(section => section.classList.remove('active'));
        document.getElementById(`${target}-section`).classList.add('active');

        // Load data
        if (target === 'device') {
            loadDevices();
        } else if (target === 'file') {
            loadFiles();
        } else if (target === 'gbdomain') {
            loadGbDomains();
        } else if (target === 'webrtc') {
            loadWebRTCSessions();
        } else if (target === 'rtmp') {
            loadRtmpStreams();
        }
    });
});

// Show Add Device Modal
function showAddDeviceModal() {
    document.getElementById('add-device-modal').classList.add('active');
    // Reset form
    document.getElementById('device-protocol').value = '2';
    document.getElementById('device-name').value = '';
    document.getElementById('device-url').value = '';
    document.getElementById('device-ipaddr').value = '';
    document.getElementById('device-user').value = '';
    document.getElementById('device-pass').value = '';
    onProtocolChange();
}

// Close Add Device Modal
function closeAddDeviceModal() {
    document.getElementById('add-device-modal').classList.remove('active');
}

// Handle protocol change
function onProtocolChange() {
    const protocol = document.getElementById('device-protocol').value;
    const urlGroup = document.getElementById('url-group');
    const ipaddrGroup = document.getElementById('ipaddr-group');
    const userGroup = document.getElementById('user-group');
    const passGroup = document.getElementById('pass-group');

    if (protocol === '2') {
        // RTSP: show URL, hide ipAddr/user/pass
        urlGroup.style.display = 'block';
        ipaddrGroup.style.display = 'none';
        userGroup.style.display = 'none';
        passGroup.style.display = 'none';
    } else if (protocol === '4') {
        // ONVIF: hide URL, show ipAddr/user/pass
        urlGroup.style.display = 'none';
        ipaddrGroup.style.display = 'block';
        userGroup.style.display = 'block';
        passGroup.style.display = 'block';
    }
}

// Submit Add Device
async function submitAddDevice() {
    const protocol = parseInt(document.getElementById('device-protocol').value);
    const name = document.getElementById('device-name').value.trim();
    const url = document.getElementById('device-url').value.trim();
    const ipAddr = document.getElementById('device-ipaddr').value.trim();
    const user = document.getElementById('device-user').value.trim();
    const pass = document.getElementById('device-pass').value.trim();

    // Validate based on protocol
    if (protocol === 2) {
        // RTSP: name and url required
        if (!name) {
            alert(t('nameRequired'));
            return;
        }
        if (!url) {
            alert(t('urlRequired'));
            return;
        }
    } else if (protocol === 4) {
        // ONVIF: name, ipAddr, user, pass required
        if (!name) {
            alert(t('nameRequired'));
            return;
        }
        if (!ipAddr) {
            alert(t('ipRequired'));
            return;
        }
        if (!user) {
            alert(t('usernameRequired'));
            return;
        }
        if (!pass) {
            alert(t('passwordRequired'));
            return;
        }
    }

    // Build request body
    const body = {
        protocol: protocol,
        name: name
    };

    if (protocol === 2) {
        body.url = url;
    } else if (protocol === 4) {
        body.ipAddr = ipAddr;
        body.user = user;
        body.pass = pass;
    }

    try {
        const response = await fetch(`${API_BASE_URL}/device`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(body)
        });
        const data = await response.json();

        if (data.code === 0) {
            alert(t('deviceAddedSuccess'));
            closeAddDeviceModal();
            loadDevices();
        } else {
            alert(t('addFailed') + ': ' + (data.msg || 'Unknown error'));
        }
    } catch (error) {
        alert('Error: ' + error.message);
    }
}

// Close modal on background click
document.getElementById('add-device-modal').addEventListener('click', function (e) {
    if (e.target === this) {
        closeAddDeviceModal();
    }
});

// Load devices from API
async function loadDevices() {
    const container = document.getElementById('device-content');
    container.innerHTML = '<div class="loading"><div class="spinner"></div></div>';

    try {
        const response = await fetch(`${API_BASE_URL}/device`);
        const data = await response.json();

        if (data.code === 0 && data.result && data.result.length > 0) {
            let html = `
                        <table>
                            <thead>
                                <tr>
                                    <th>${t('deviceId')}</th>
                                    <th>${t('name')}</th>
                                    <th>${t('protocol')}</th>
                                    <th>${t('type')}</th>
                                    <th>${t('status')}</th>
                                    <th>${t('codec')}</th>
                                    <th>${t('resolution')}</th>
                                    <th>${t('action')}</th>
                                </tr>
                            </thead>
                            <tbody>
                    `;

            data.result.forEach(device => {
                const statusClass = device.status === 'ON' ? 'status-on' : 'status-off';
                html += `
                            <tr>
                                <td>${device.deviceId || '-'}</td>
                                <td>${device.name || '-'}</td>
                                <td>${getProtocolName(device.protocol)}</td>
                                <td>${device.type || '-'}</td>
                                <td><span class="status-badge ${statusClass}">${device.status || '-'}</span></td>
                                <td>${device.codec || '-'}</td>
                                <td>${device.resolution || '-'}</td>
                                <td>
                                    <span class="preview-icon" onclick="previewDevice('${device.deviceId}', '${device.name || 'Device Preview'}')" title="Preview">
                                        <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M14.752 11.168l-3.197-2.132A1 1 0 0010 9.87v4.263a1 1 0 001.555.832l3.197-2.132a1 1 0 000-1.664z" />
                                            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
                                        </svg>
                                    </span>
                                    <span class="delete-icon" onclick="deleteDevice('${device.deviceId}', '${device.name || 'this device'}')" title="Delete">
                                        <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16" />
                                        </svg>
                                    </span>
                                </td>
                            </tr>
                        `;
            });

            html += '</tbody></table>';
            container.innerHTML = html;
        } else if (data.code === 0) {
            container.innerHTML = `
                        <div class="empty-state">
                            <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M15 10l4.553-2.276A1 1 0 0121 8.618v6.764a1 1 0 01-1.447.894L15 14M5 18h8a2 2 0 002-2V8a2 2 0 00-2-2H5a2 2 0 00-2 2v8a2 2 0 002 2z" />
                            </svg>
                            <p>${t('noDevicesFound')}</p>
                        </div>
                    `;
        } else {
            container.innerHTML = `<div class="error-message">Error: ${data.msg || 'Failed to load devices'}</div>`;
        }
    } catch (error) {
        container.innerHTML = `<div class="error-message">Error: ${error.message}</div>`;
    }
}

// Delete device
async function deleteDevice(deviceId, deviceName) {
    if (!confirm(`${t('confirmDeleteDevice')} "${deviceName}"?`)) {
        return;
    }

    try {
        const response = await fetch(`${API_BASE_URL}/device`, {
            method: 'DELETE',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ device: [deviceId] })
        });
        const data = await response.json();

        if (data.code === 0) {
            alert(t('deviceDeletedSuccess'));
            // Reload device list
            loadDevices();
        } else {
            alert(t('deleteFailed') + ': ' + (data.msg || 'Unknown error'));
        }
    } catch (error) {
        alert(t('deleteFailed') + ': ' + error.message);
    }
}

// Upload file
async function uploadFile(input) {
    if (!input.files || input.files.length === 0) {
        return;
    }

    const file = input.files[0];
    const formData = new FormData();
    formData.append('file', file);

    // Show loading state
    const container = document.getElementById('file-content');
    const originalContent = container.innerHTML;
    container.innerHTML = `
                <div class="loading">
                    <div class="spinner"></div>
                    <p style="margin-top: 15px; color: #6c757d;">${t('uploading')} ${escapeHtml(file.name)}...</p>
                </div>
            `;

    try {
        const response = await fetch(`${API_BASE_URL}/file/upload`, {
            method: 'POST',
            body: formData
        });
        const data = await response.json();

        if (data.code === 0) {
            alert(t('uploadSuccess') + ': ' + (data.msg || file.name));
            // Reload file list
            loadFiles();
        } else {
            alert(t('uploadFailed') + ': ' + (data.msg || 'Unknown error'));
            container.innerHTML = originalContent;
        }
    } catch (error) {
        alert(t('uploadFailed') + ': ' + error.message);
        container.innerHTML = originalContent;
    } finally {
        // Reset file input
        input.value = '';
    }
}

// Delete file
async function deleteFile(fileId, fileName) {
    if (!confirm(`${t('confirmDeleteFile')} "${fileName}" ? `)) {
        return;
    }

    try {
        const response = await fetch(`${API_BASE_URL}/file`, {
            method: 'DELETE',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ fileId: Number(fileId) })
        });
        const data = await response.json();

        if (data.code === 0) {
            alert(t('fileDeletedSuccess'));
            // Reload file list
            loadFiles();
        } else {
            alert(t('deleteFailed') + ': ' + (data.msg || 'Unknown error'));
        }
    } catch (error) {
        alert(t('deleteFailed') + ': ' + error.message);
    }
}

// Load files from API
async function loadFiles() {
    const container = document.getElementById('file-content');
    container.innerHTML = '<div class="loading"><div class="spinner"></div></div>';

    try {
        const response = await fetch(`${API_BASE_URL}/file`);
        const data = await response.json();

        if (data.code === 0 && data.result && data.result.length > 0) {
            let html = `
                        <table>
                            <thead>
                                <tr>
                                    <th>${t('fileId')}</th>
                                    <th>${t('fileName')}</th>
                                    <th>${t('size')}</th>
                                    <th>${t('codec')}</th>
                                    <th>${t('resolution')}</th>
                                    <th>${t('duration')}</th>
                                    <th>${t('frameRate')}</th>
                                    <th>${t('action')}</th>
                                </tr>
                            </thead>
                            <tbody>
                    `;

            data.result.forEach(file => {
                const sizeStr = formatFileSize(file.size);
                const durationStr = formatDuration(file.duration);
                html += `
                            <tr>
                                <td>${file.fileId || '-'}</td>
                                <td>${file.fileName || '-'}</td>
                                <td>${sizeStr}</td>
                                <td>${file.codec || '-'}</td>
                                <td>${file.resolution || '-'}</td>
                                <td>${durationStr}</td>
                                <td>${file.frameRate ? file.frameRate.toFixed(1) + ' fps' : '-'}</td>
                                <td>
                                    <span class="preview-icon" onclick="previewFile('${file.fileId}', '${file.fileName || 'File Preview'}')" title="Preview">
                                        <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M14.752 11.168l-3.197-2.132A1 1 0 0010 9.87v4.263a1 1 0 001.555.832l3.197-2.132a1 1 0 000-1.664z" />
                                            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
                                        </svg>
                                    </span>
                                    <span class="delete-icon" onclick="deleteFile('${file.fileId}', '${file.fileName || 'this file'}')" title="Delete">
                                        <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16" />
                                        </svg>
                                    </span>
                                </td>
                            </tr>
                        `;
            });

            html += '</tbody></table>';
            container.innerHTML = html;
        } else if (data.code === 0) {
            container.innerHTML = `
                        <div class="empty-state">
                            <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M7 21h10a2 2 0 002-2V9.414a1 1 0 00-.293-.707l-5.414-5.414A1 1 0 0012.586 3H7a2 2 0 00-2 2v14a2 2 0 002 2z" />
                            </svg>
                            <p>${t('noFilesFound')}</p>
                        </div>
                    `;
        } else {
            container.innerHTML = `<div class="error-message">Error: ${data.msg || 'Failed to load files'}</div>`;
        }
    } catch (error) {
        container.innerHTML = `<div class="error-message">Error: ${error.message}</div>`;
    }
}

// Load GB Domains from API
async function loadGbDomains() {
    const container = document.getElementById('gbdomain-content');
    container.innerHTML = '<div class="loading"><div class="spinner"></div></div>';

    try {
        const response = await fetch(`${API_BASE_URL}/gb/domain`);
        const data = await response.json();

        if (data.code === 0 && data.result && data.result.length > 0) {
            let html = `
                        <table>
                            <thead>
                                <tr>
                                    <th>${t('domainId')}</th>
                                    <th>${t('deviceCount')}</th>
                                    <th>${t('ip')}</th>
                                    <th>${t('port')}</th>
                                </tr>
                            </thead>
                            <tbody>
                    `;

            data.result.forEach(domain => {
                html += `
                            <tr>
                                <td>${domain.id || '-'}</td>
                                <td>${domain.devNum || 0}</td>
                                <td>${domain.ip || '-'}</td>
                                <td>${domain.port || '-'}</td>
                            </tr>
                        `;
            });

            html += '</tbody></table>';
            container.innerHTML = html;
        } else if (data.code === 0) {
            container.innerHTML = `
                        <div class="empty-state">
                            <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M19 21V5a2 2 0 00-2-2H7a2 2 0 00-2 2v16m14 0h2m-2 0h-5m-9 0H3m2 0h5M9 7h1m-1 4h1m4-4h1m-1 4h1m-5 10v-5a1 1 0 011-1h2a1 1 0 011 1v5m-4 0h4" />
                            </svg>
                            <p>${t('noGbDomainsFound')}</p>
                        </div>
                    `;
        } else {
            container.innerHTML = `<div class="error-message">Error: ${data.msg || 'Failed to load GB domains'}</div>`;
        }
    } catch (error) {
        container.innerHTML = `<div class="error-message">Error: ${error.message}</div>`;
    }
}

// Sync GB Catalog
async function syncGbCatalog() {
    try {
        const response = await fetch(`${API_BASE_URL}/gb/catalog`);
        const data = await response.json();

        if (data.code === 0) {
            alert(t('catalogSyncSuccess'));
            // Reload domains and devices
            loadGbDomains();
            loadDevices();
        } else {
            alert(t('catalogSyncFailed') + ': ' + (data.msg || 'Unknown error'));
        }
    } catch (error) {
        alert('Error: ' + error.message);
    }
}

// Show GB Server Info
async function showGbServerInfo() {
    try {
        const response = await fetch(`${API_BASE_URL}/gb/server`);
        const data = await response.json();

        if (data.code === 0 && data.result) {
            const info = data.result;
            const rtpTransportMap = {
                0: 'UDP',
                1: 'TCP Active',
                2: 'TCP Passive'
            };
            const rtpTransport = rtpTransportMap[info.rtpTransport] || info.rtpTransport;

            alert(
                `${t('gbServerInfo')}:\n\n` +
                `ID: ${info.id || '-'}\n` +
                `IP: ${info.ip || '-'}\n` +
                `${t('port')}: ${info.port || '-'}\n` +
                `${t('password')}: ${info.pass || '-'}\n` +
                `${t('rtpTransport')}: ${rtpTransport}`
            );
        } else {
            alert(t('failedToGetServerInfo') + ': ' + (data.msg || 'Unknown error'));
        }
    } catch (error) {
        alert('Error: ' + error.message);
    }
}

// GB Tabs navigation
document.querySelectorAll('.gb-tab').forEach(tab => {
    tab.addEventListener('click', function () {
        // Update active tab
        document.querySelectorAll('.gb-tab').forEach(t => t.classList.remove('active'));
        this.classList.add('active');

        // Show corresponding content
        const target = this.getAttribute('data-gb-target');
        document.querySelectorAll('.gb-tab-content').forEach(content => content.classList.remove('active'));
        document.getElementById(target).classList.add('active');

        // Load GB devices when switching to record tab
        if (target === 'gb-record-tab') {
            loadGbDevicesForRecord();
        }
    });
});

// Device Tabs navigation
document.querySelectorAll('.device-tab').forEach(tab => {
    tab.addEventListener('click', function () {
        // Update active tab
        document.querySelectorAll('.device-tab').forEach(t => t.classList.remove('active'));
        this.classList.add('active');

        // Show corresponding content
        const target = this.getAttribute('data-device-target');
        document.querySelectorAll('.device-tab-content').forEach(content => content.classList.remove('active'));
        document.getElementById(target).classList.add('active');

        // Load PTZ devices when switching to PTZ control tab
        if (target === 'ptz-control-tab') {
            loadPTZDevices();
        }
    });
});

// Cached GB devices list
let cachedGbDevices = [];

// Load GB devices (protocol==1 and type=='camera') for record search
async function loadGbDevicesForRecord() {
    const select = document.getElementById('gb-record-device');

    try {
        const response = await fetch(`${API_BASE_URL}/device`);
        const data = await response.json();

        if (data.code === 0 && data.result) {
            // Filter GB devices (protocol==1) with type=='camera'
            cachedGbDevices = data.result.filter(d => d.protocol === 1 && d.type === 'camera');

            // Populate select
            let options = `<option value="">${t('selectDevice')}</option>`;
            cachedGbDevices.forEach(device => {
                const displayName = device.name ? `${device.name} (${device.deviceId})` : device.deviceId;
                options += `<option value="${device.deviceId}">${displayName}</option>`;
            });
            select.innerHTML = options;
        }
    } catch (error) {
        console.error('Error loading GB devices:', error);
    }
}

// Get record type display name
function getRecordTypeName(type) {
    const map = {
        'time': t('recordTypeTime'),
        'alarm': t('recordTypeAlarm'),
        'manual': t('recordTypeManual'),
        'all': t('typeAll')
    };
    return map[type] || type || '-';
}

// Record type mapping (deprecated, use getRecordTypeName)
const RECORD_TYPE_MAP = {
    'time': 'Time (Scheduled)',
    'alarm': 'Alarm',
    'manual': 'Manual',
    'all': 'All'
};

// Search GB Records
async function searchGbRecords() {
    const deviceId = document.getElementById('gb-record-device').value;
    const startTime = document.getElementById('gb-record-start').value;
    const endTime = document.getElementById('gb-record-end').value;
    const type = document.getElementById('gb-record-type').value;
    const container = document.getElementById('gb-record-content');

    // Validate inputs
    if (!deviceId) {
        alert(t('selectDeviceAlert'));
        return;
    }
    if (!startTime) {
        alert(t('selectStartTime'));
        return;
    }
    if (!endTime) {
        alert(t('selectEndTime'));
        return;
    }

    // Validate time range (<= 1 hour)
    const start = new Date(startTime);
    const end = new Date(endTime);
    const diffMs = end - start;
    const diffHours = diffMs / (1000 * 60 * 60);

    if (diffMs <= 0) {
        alert(t('endTimeAfterStart'));
        return;
    }
    if (diffHours > 1) {
        alert(t('timeRangeLimit'));
        return;
    }

    // Show loading
    container.innerHTML = '<div class="loading"><div class="spinner"></div></div>';

    try {
        // Format times for API (ISO 8601)
        const formatDateTime = (dt) => {
            return dt.replace(' ', 'T') + ':00';
        };

        const body = {
            deviceId: deviceId,
            startTime: formatDateTime(startTime),
            endTime: formatDateTime(endTime),
            type: type
        };

        const response = await fetch(`${API_BASE_URL}/gb/record`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(body)
        });
        const data = await response.json();

        if (data.code === 0 && data.result && data.result.length > 0) {
            let html = `
                        <table>
                            <thead>
                                <tr>
                                    <th>${t('deviceId')}</th>
                                    <th>${t('name')}</th>
                                    <th>${t('startTime')}</th>
                                    <th>${t('endTime')}</th>
                                    <th>${t('type')}</th>
                                    <th>${t('action')}</th>
                                </tr>
                            </thead>
                            <tbody>
                    `;

            data.result.forEach(record => {
                const recordType = getRecordTypeName(record.type);
                html += `
                            <tr>
                                <td>${record.deviceId || '-'}</td>
                                <td>${record.name || '-'}</td>
                                <td>${record.startTime || '-'}</td>
                                <td>${record.endTime || '-'}</td>
                                <td>${recordType}</td>
                                <td>
                                    <span class="preview-icon" onclick="playGbRecord('${record.deviceId}', '${record.startTime}', '${record.endTime}', '${record.type || 'time'}')" title="Play">
                                        <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M14.752 11.168l-3.197-2.132A1 1 0 0010 9.87v4.263a1 1 0 001.555.832l3.197-2.132a1 1 0 000-1.664z" />
                                            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
                                        </svg>
                                    </span>
                                </td>
                            </tr>
                        `;
            });

            html += '</tbody></table>';
            container.innerHTML = html;
        } else if (data.code === 0) {
            container.innerHTML = `
                        <div class="empty-state">
                            <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M21 21l-6-6m2-5a7 7 0 11-14 0 7 7 0 0114 0z" />
                            </svg>
                            <p>${t('noRecordsFound')}</p>
                        </div>
                    `;
        } else {
            container.innerHTML = `<div class="error-message">Error: ${data.msg || 'Failed to search records'}</div>`;
        }
    } catch (error) {
        container.innerHTML = `<div class="error-message">Error: ${error.message}</div>`;
    }
}

// Play GB Record
async function playGbRecord(deviceId, startTime, endTime, type) {
    const modal = document.getElementById('video-modal');
    const videoElement = document.getElementById('video-player');
    const videoTitle = document.getElementById('video-title');
    const videoLoading = document.getElementById('video-loading');

    // Show modal and loading
    modal.classList.add('active');
    videoTitle.textContent = `Record: ${startTime} - ${endTime}`;
    videoLoading.style.display = 'block';

    try {
        // Get record playback URL
        const body = {
            deviceId: deviceId,
            startTime: startTime,
            endTime: endTime,
            type: type
        };

        const response = await fetch(`${API_BASE_URL}/gb/record/url`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify(body)
        });
        const data = await response.json();

        if (data.code === 0 && data.result && data.result.httpFlvUrl) {
            const flvUrl = data.result.httpFlvUrl;

            // Destroy existing player if any
            if (mpegtsPlayer) {
                mpegtsPlayer.destroy();
                mpegtsPlayer = null;
            }

            // Check if mpegts.js is supported
            if (mpegts.isSupported()) {
                mpegtsPlayer = mpegts.createPlayer({
                    type: 'flv',
                    url: flvUrl,
                    isLive: true,
                    liveSync: true,
                    liveSyncTargetLatency: 0.5,
                    enableWorker: true
                });
                mpegtsPlayer.attachMediaElement(videoElement);
                mpegtsPlayer.load();
                mpegtsPlayer.play();
            } else {
                alert(t('browserNotSupported'));
                closeVideoModal();
            }
        } else {
            alert(t('failedToGetRecordUrl') + ': ' + (data.msg || 'Unknown error'));
            closeVideoModal();
        }
    } catch (error) {
        alert('Error: ' + error.message);
        closeVideoModal();
    } finally {
        videoLoading.style.display = 'none';
    }
}

// Video player instance
let mpegtsPlayer = null;

// Preview device stream
async function previewDevice(deviceId, deviceName) {
    const modal = document.getElementById('video-modal');
    const videoElement = document.getElementById('video-player');
    const videoTitle = document.getElementById('video-title');
    const videoLoading = document.getElementById('video-loading');

    // Show modal and loading
    modal.classList.add('active');
    videoTitle.textContent = deviceName;
    videoLoading.style.display = 'block';

    try {
        // Get device URL
        const response = await fetch(`${API_BASE_URL}/device/url?deviceId=${encodeURIComponent(deviceId)}`);
        const data = await response.json();

        if (data.code === 0 && data.result && data.result.httpFlvUrl) {
            const flvUrl = data.result.httpFlvUrl;

            // Destroy existing player if any
            if (mpegtsPlayer) {
                mpegtsPlayer.destroy();
                mpegtsPlayer = null;
            }

            // Close existing peer connection if any
            if (whepPeerConnection) {
                whepPeerConnection.close();
                whepPeerConnection = null;
            }

            // Check if mpegts.js is supported
            if (mpegts.isSupported()) {
                mpegtsPlayer = mpegts.createPlayer({
                    type: 'flv',
                    url: flvUrl,
                    isLive: true,
                    liveSync: true,
                    liveSyncTargetLatency: 0.5,
                    enableWorker: true
                });
                mpegtsPlayer.attachMediaElement(videoElement);
                mpegtsPlayer.load();
                mpegtsPlayer.play();
            } else {
                alert(t('browserNotSupported'));
                closeVideoModal();
            }
        } else {
            alert(t('failedToGetDeviceUrl') + ': ' + (data.msg || 'Unknown error'));
            closeVideoModal();
        }
    } catch (error) {
        alert('Error: ' + error.message);
        closeVideoModal();
    } finally {
        videoLoading.style.display = 'none';
    }
}

// Global variable to store current device ID for PTZ control
let currentPTZDeviceId = null;

// Preview device stream with PTZ support
async function previewDeviceWithPTZ(deviceId, deviceName, hasPTZ) {
    currentPTZDeviceId = hasPTZ ? deviceId : null;

    // Show/hide PTZ panel
    const ptzPanel = document.getElementById('ptz-panel');
    if (hasPTZ) {
        ptzPanel.classList.add('active');
        // Load presets for this device
        loadPresets();
    } else {
        ptzPanel.classList.remove('active');
    }

    // Call original preview function
    await previewDevice(deviceId, deviceName);
}

// Send PTZ command
async function sendPTZCommand(cmd) {
    if (!currentPTZDeviceId) {
        return;
    }

    // Get timeout value from input
    const timeoutInput = document.getElementById('ptz-timeout-input');
    const timeout = timeoutInput ? parseInt(timeoutInput.value) || 500 : 500;

    try {
        const response = await fetch(`${API_BASE_URL}/device/ptz`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                deviceId: currentPTZDeviceId,
                ptzCmd: cmd,
                timeout: timeout
            })
        });
        const data = await response.json();

        if (data.code !== 0) {
            console.error('PTZ command failed:', data.msg);
        }
    } catch (error) {
        console.error('PTZ command error:', error);
    }
}

// Global variable for selected preset
let selectedPresetId = null;
let currentPresetList = [];

// Load presets from API
async function loadPresets() {
    if (!currentPTZDeviceId) {
        return;
    }

    const listContainer = document.getElementById('ptz-preset-list');
    listContainer.innerHTML = '<div class="ptz-preset-empty">Loading...</div>';

    try {
        const response = await fetch(`${API_BASE_URL}/device/preset?deviceId=${encodeURIComponent(currentPTZDeviceId)}`);
        const data = await response.json();

        if (data.code === 0 && data.result && data.result.length > 0) {
            currentPresetList = data.result;
            let html = '';
            data.result.forEach(preset => {
                html += `
                            <div class="ptz-preset-item" onclick="selectPreset('${preset.presetID}', this)" data-preset-id="${preset.presetID}">
                                ${preset.name || 'Preset ' + preset.presetID}
                            </div>
                        `;
            });
            listContainer.innerHTML = html;
        } else {
            currentPresetList = [];
            listContainer.innerHTML = '<div class="ptz-preset-empty">No presets available</div>';
        }
    } catch (error) {
        console.error('Load presets error:', error);
        listContainer.innerHTML = '<div class="ptz-preset-empty">Error loading presets</div>';
    }
}

// Select preset
function selectPreset(presetId, element) {
    // Remove previous selection
    document.querySelectorAll('.ptz-preset-item').forEach(item => {
        item.classList.remove('selected');
    });
    // Add selection to clicked item
    element.classList.add('selected');
    selectedPresetId = presetId;
}

// Refresh preset list
function refreshPresets() {
    selectedPresetId = null;
    loadPresets();
}

// Go to preset
async function gotoPreset() {
    if (!currentPTZDeviceId) {
        alert('No device selected');
        return;
    }
    if (selectedPresetId === null) {
        alert('Please select a preset first');
        return;
    }

    // Get timeout value from input
    const timeoutInput = document.getElementById('ptz-timeout-input');
    const timeout = timeoutInput ? parseInt(timeoutInput.value) || 500 : 500;

    try {
        const response = await fetch(`${API_BASE_URL}/device/ptz`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                deviceId: currentPTZDeviceId,
                ptzCmd: 7,
                presetID: String(selectedPresetId),
                timeout: timeout
            })
        });
        const data = await response.json();

        if (data.code === 0) {
            console.log('Goto preset success');
        } else {
            alert('Goto preset failed: ' + (data.msg || 'Unknown error'));
        }
    } catch (error) {
        alert('Error: ' + error.message);
    }
}

// Add preset
async function addPreset() {
    if (!currentPTZDeviceId) {
        alert('No device selected');
        return;
    }

    // Determine presetID to use
    let presetId;
    if (selectedPresetId !== null) {
        // Use currently selected preset ID
        presetId = selectedPresetId;
    } else if (currentPresetList.length > 0) {
        // Find max presetID and add 1
        const maxPresetId = Math.max(...currentPresetList.map(p => parseInt(p.presetID) || 0));
        presetId = String(maxPresetId + 1);
    } else {
        // Empty list, use '1'
        presetId = '1';
    }

    // Get timeout value from input
    const timeoutInput = document.getElementById('ptz-timeout-input');
    const timeout = timeoutInput ? parseInt(timeoutInput.value) || 500 : 500;

    try {
        const response = await fetch(`${API_BASE_URL}/device/ptz`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                deviceId: currentPTZDeviceId,
                ptzCmd: 8,
                presetID: String(presetId),
                timeout: timeout
            })
        });
        const data = await response.json();

        if (data.code === 0) {
            alert('Preset added successfully');
            loadPresets();
        } else {
            alert('Add preset failed: ' + (data.msg || 'Unknown error'));
        }
    } catch (error) {
        alert('Error: ' + error.message);
    }
}

// Delete preset
async function deletePreset() {
    if (!currentPTZDeviceId) {
        alert('No device selected');
        return;
    }
    if (selectedPresetId === null) {
        alert('Please select a preset first');
        return;
    }

    if (!confirm('Are you sure you want to delete this preset?')) {
        return;
    }

    // Get timeout value from input
    const timeoutInput = document.getElementById('ptz-timeout-input');
    const timeout = timeoutInput ? parseInt(timeoutInput.value) || 500 : 500;

    try {
        const response = await fetch(`${API_BASE_URL}/device/ptz`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                deviceId: currentPTZDeviceId,
                ptzCmd: 9,
                presetID: String(selectedPresetId),
                timeout: timeout
            })
        });
        const data = await response.json();

        if (data.code === 0) {
            alert('Preset deleted successfully');
            selectedPresetId = null;
            loadPresets();
        } else {
            alert('Delete preset failed: ' + (data.msg || 'Unknown error'));
        }
    } catch (error) {
        alert('Error: ' + error.message);
    }
}

// Load PTZ-capable devices
async function loadPTZDevices() {
    const container = document.getElementById('ptz-device-content');
    container.innerHTML = '<div class=\"loading\"><div class=\"spinner\"></div></div>';

    try {
        const response = await fetch(`${API_BASE_URL}/device`);
        const data = await response.json();

        if (data.code === 0 && data.result && data.result.length > 0) {
            // Filter PTZ-capable devices
            // Criteria: (onvifprofile && onvifptzurl) OR (ptztype != 0 && ptztype != 3)
            const ptzDevices = data.result.filter(device => {
                const hasOnvifPTZ = device.onvifProfile && device.onvifPtzUrl;
                const hasPTZType = device.ptzType && device.ptzType !== 0 && device.ptzType !== 3;
                return hasOnvifPTZ || hasPTZType;
            });

            if (ptzDevices.length > 0) {
                let html = `
                            <table>
                                <thead>
                                    <tr>
                                        <th>${t('deviceId')}</th>
                                        <th>${t('name')}</th>
                                        <th>${t('protocol')}</th>
                                        <th>${t('type')}</th>
                                        <th>${t('status')}</th>
                                        <th>${t('action')}</th>
                                    </tr>
                                </thead>
                                <tbody>
                        `;

                ptzDevices.forEach(device => {
                    const statusClass = device.status === 'ON' ? 'status-on' : 'status-off';
                    html += `
                                <tr>
                                    <td>${device.deviceId || '-'}</td>
                                    <td>${device.name || '-'}</td>
                                    <td>${getProtocolName(device.protocol)}</td>
                                    <td>${device.type || '-'}</td>
                                    <td><span class=\"status-badge ${statusClass}\">${device.status || '-'}</span></td>
                                    <td>
                                        <span class=\"preview-icon\" onclick=\"previewDeviceWithPTZ('${device.deviceId}', '${device.name || 'Device Preview'}', true)\" title=\"Preview with PTZ\">
                                            <svg xmlns=\"http://www.w3.org/2000/svg\" width=\"20\" height=\"20\" fill=\"none\" viewBox=\"0 0 24 24\" stroke=\"currentColor\">
                                                <path stroke-linecap=\"round\" stroke-linejoin=\"round\" stroke-width=\"2\" d=\"M14.752 11.168l-3.197-2.132A1 1 0 0010 9.87v4.263a1 1 0 001.555.832l3.197-2.132a1 1 0 000-1.664z\" />
                                                <path stroke-linecap=\"round\" stroke-linejoin=\"round\" stroke-width=\"2\" d=\"M21 12a9 9 0 11-18 0 9 9 0 0118 0z\" />
                                            </svg>
                                        </span>
                                    </td>
                                </tr>
                            `;
                });

                html += '</tbody></table>';
                container.innerHTML = html;
            } else {
                container.innerHTML = `
                            <div class=\"empty-state\">
                                <svg xmlns=\"http://www.w3.org/2000/svg\" fill=\"none\" viewBox=\"0 0 24 24\" stroke=\"currentColor\">
                                    <path stroke-linecap=\"round\" stroke-linejoin=\"round\" stroke-width=\"2\" d=\"M15 10l4.553-2.276A1 1 0 0121 8.618v6.764a1 1 0 01-1.447.894L15 14M5 18h8a2 2 0 002-2V8a2 2 0 00-2-2H5a2 2 0 00-2 2v8a2 2 0 002 2z\" />
                                </svg>
                                <p>${t('noPTZDevicesFound')}</p>
                            </div>
                        `;
            }
        } else if (data.code === 0) {
            container.innerHTML = `
                        <div class=\"empty-state\">
                            <svg xmlns=\"http://www.w3.org/2000/svg\" fill=\"none\" viewBox=\"0 0 24 24\" stroke=\"currentColor\">
                                <path stroke-linecap=\"round\" stroke-linejoin=\"round\" stroke-width=\"2\" d=\"M15 10l4.553-2.276A1 1 0 0121 8.618v6.764a1 1 0 01-1.447.894L15 14M5 18h8a2 2 0 002-2V8a2 2 0 00-2-2H5a2 2 0 00-2 2v8a2 2 0 002 2z\" />
                            </svg>
                            <p>${t('noDevicesFound')}</p>
                        </div>
                    `;
        } else {
            container.innerHTML = `<div class=\"error-message\">Error: ${data.msg || 'Failed to load devices'}</div>`;
        }
    } catch (error) {
        container.innerHTML = `<div class=\"error-message\">Error: ${error.message}</div>`;
    }
}

// Close video modal
function closeVideoModal() {
    const modal = document.getElementById('video-modal');
    modal.classList.remove('active');

    // Hide PTZ panel
    const ptzPanel = document.getElementById('ptz-panel');
    ptzPanel.classList.remove('active');
    currentPTZDeviceId = null;
    selectedPresetId = null;

    // Destroy player
    if (mpegtsPlayer) {
        mpegtsPlayer.destroy();
        mpegtsPlayer = null;
    }
}

// Close modal on background click
document.getElementById('video-modal').addEventListener('click', function (e) {
    if (e.target === this) {
        closeVideoModal();
    }
});

// Preview file stream
async function previewFile(fileId, fileName) {
    const modal = document.getElementById('video-modal');
    const videoElement = document.getElementById('video-player');
    const videoTitle = document.getElementById('video-title');
    const videoLoading = document.getElementById('video-loading');

    // Show modal and loading
    modal.classList.add('active');
    videoTitle.textContent = fileName;
    videoLoading.style.display = 'block';

    try {
        // Get file URL
        const response = await fetch(`${API_BASE_URL}/file/url?fileId=${encodeURIComponent(fileId)}`);
        const data = await response.json();

        if (data.code === 0 && data.result && data.result.httpFlvUrl) {
            const flvUrl = data.result.httpFlvUrl;

            // Destroy existing player if any
            if (mpegtsPlayer) {
                mpegtsPlayer.destroy();
                mpegtsPlayer = null;
            }

            // Check if mpegts.js is supported
            if (mpegts.isSupported()) {
                mpegtsPlayer = mpegts.createPlayer({
                    type: 'flv',
                    url: flvUrl,
                    isLive: true,
                    liveSync: true,
                    liveSyncTargetLatency: 0.5,
                    enableWorker: true
                });
                mpegtsPlayer.attachMediaElement(videoElement);
                mpegtsPlayer.load();
                mpegtsPlayer.play();
            } else {
                alert(t('browserNotSupported'));
                closeVideoModal();
            }
        } else {
            alert(t('failedToGetFileUrl') + ': ' + (data.msg || 'Unknown error'));
            closeVideoModal();
        }
    } catch (error) {
        alert('Error: ' + error.message);
        closeVideoModal();
    } finally {
        videoLoading.style.display = 'none';
    }
}

// Load WebRTC sessions from API
async function loadWebRTCSessions() {
    const container = document.getElementById('webrtc-content');
    container.innerHTML = '<div class="loading"><div class="spinner"></div></div>';

    try {
        const response = await fetch(`${API_BASE_URL}/rtc/session`);
        const data = await response.json();

        if (data.code === 0 && data.result && data.result.length > 0) {
            let html = `
                        <table>
                            <thead>
                                <tr>
                                    <th>${t('sessionId')}</th>
                                    <th>${t('videoCodec')}</th>
                                    <th>${t('audioCodec')}</th>
                                    <th>${t('action')}</th>
                                </tr>
                            </thead>
                            <tbody>
                    `;

            data.result.forEach(session => {
                html += `
                            <tr>
                                <td>${session.sessionId || '-'}</td>
                                <td>${session.videoCodec || '-'}</td>
                                <td>${session.audioCodec || '-'}</td>
                                <td>
                                    <span class="preview-icon" onclick="previewWebRTC('${session.sessionId}', 'Session ${session.sessionId || ''}')" title="Preview">
                                        <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M14.752 11.168l-3.197-2.132A1 1 0 0010 9.87v4.263a1 1 0 001.555.832l3.197-2.132a1 1 0 000-1.664z" />
                                            <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
                                        </svg>
                                    </span>
                                </td>
                            </tr>
                        `;
            });

            html += '</tbody></table>';
            container.innerHTML = html;
        } else if (data.code === 0) {
            container.innerHTML = `
                        <div class="empty-state">
                            <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M8.111 16.404a5.5 5.5 0 017.778 0M12 20h.01m-7.08-7.071c3.904-3.905 10.236-3.905 14.141 0M1.394 9.393c5.857-5.857 15.355-5.857 21.213 0" />
                            </svg>
                            <p>${t('noSessionsFound')}</p>
                        </div>
                    `;
        } else {
            container.innerHTML = `<div class="error-message">Error: ${data.msg || 'Failed to load WebRTC sessions'}</div>`;
        }
    } catch (error) {
        container.innerHTML = `<div class="error-message">Error: ${error.message}</div>`;
    }
}

// WebRTC peer connection
let whepPeerConnection = null;

// Preview WebRTC stream using WHEP
async function previewWebRTC(sessionId, sessionName) {
    const modal = document.getElementById('video-modal');
    const videoElement = document.getElementById('video-player');
    const videoTitle = document.getElementById('video-title');
    const videoLoading = document.getElementById('video-loading');

    // Show modal and loading
    modal.classList.add('active');
    videoTitle.textContent = sessionName;
    videoLoading.style.display = 'block';

    try {
        // Get WebRTC session URL
        const response = await fetch(`${API_BASE_URL}/rtc/session/url?sessionId=${encodeURIComponent(sessionId)}`);
        const data = await response.json();

        if (data.code === 0 && data.result && data.result.rtcUrl) {
            const whepUrl = data.result.rtcUrl;

            // Close existing peer connection if any
            if (whepPeerConnection) {
                whepPeerConnection.close();
                whepPeerConnection = null;
            }

            // Create WHEP connection
            await startWhepPlayer(whepUrl, videoElement);
        } else {
            alert('Failed to get WebRTC URL: ' + (data.msg || 'Unknown error'));
            closeVideoModal();
        }
    } catch (error) {
        alert('Error: ' + error.message);
        closeVideoModal();
    } finally {
        videoLoading.style.display = 'none';
    }
}

// WHEP Player implementation
async function startWhepPlayer(whepUrl, videoElement) {
    // Create peer connection
    whepPeerConnection = new RTCPeerConnection({
        //iceServers: [{ urls: 'stun:stun.l.google.com:19302' }]
    });

    // Handle incoming tracks
    whepPeerConnection.ontrack = (event) => {
        if (event.streams && event.streams[0]) {
            videoElement.srcObject = event.streams[0];
        }
    };

    // Add transceivers for receiving audio and video
    whepPeerConnection.addTransceiver('video', { direction: 'recvonly' });
    whepPeerConnection.addTransceiver('audio', { direction: 'recvonly' });

    // Create offer
    const offer = await whepPeerConnection.createOffer();
    await whepPeerConnection.setLocalDescription(offer);

    // Wait for ICE gathering to complete
    await new Promise((resolve) => {
        if (whepPeerConnection.iceGatheringState === 'complete') {
            resolve();
        } else {
            const checkState = () => {
                if (whepPeerConnection.iceGatheringState === 'complete') {
                    whepPeerConnection.removeEventListener('icegatheringstatechange', checkState);
                    resolve();
                }
            };
            whepPeerConnection.addEventListener('icegatheringstatechange', checkState);
        }
    });

    // Send offer to WHEP endpoint
    const whepResponse = await fetch(whepUrl, {
        method: 'POST',
        headers: {
            'Content-Type': 'application/sdp'
        },
        body: whepPeerConnection.localDescription.sdp
    });

    if (!whepResponse.ok) {
        throw new Error('WHEP request failed: ' + whepResponse.statusText);
    }

    // Get answer SDP
    const answerSdp = await whepResponse.text();
    await whepPeerConnection.setRemoteDescription({
        type: 'answer',
        sdp: answerSdp
    });
}

// Update closeVideoModal to also close WHEP connection
const originalCloseVideoModal = closeVideoModal;
closeVideoModal = function () {
    // Close WHEP peer connection
    if (whepPeerConnection) {
        whepPeerConnection.close();
        whepPeerConnection = null;
    }
    // Clear video srcObject for WebRTC
    const videoElement = document.getElementById('video-player');
    if (videoElement.srcObject) {
        videoElement.srcObject = null;
    }
    originalCloseVideoModal();
};

// Format file size
function formatFileSize(bytes) {
    if (!bytes) return '-';
    const units = ['B', 'KB', 'MB', 'GB', 'TB'];
    let i = 0;
    while (bytes >= 1024 && i < units.length - 1) {
        bytes /= 1024;
        i++;
    }
    return bytes.toFixed(2) + ' ' + units[i];
}

// Format duration
function formatDuration(seconds) {
    if (!seconds) return '-';
    const hours = Math.floor(seconds / 3600);
    const minutes = Math.floor((seconds % 3600) / 60);
    const secs = Math.floor(seconds % 60);

    if (hours > 0) {
        return `${hours}:${minutes.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}`;
    }
    return `${minutes}:${secs.toString().padStart(2, '0')}`;
}

// Initial load
document.addEventListener('DOMContentLoaded', function () {
    // Set initial language button state
    document.querySelectorAll('.lang-btn').forEach(btn => {
        btn.classList.toggle('active', btn.getAttribute('data-lang') === currentLang);
    });

    // Apply translations
    applyTranslations();

    // Load initial data
    loadDevices();
});

// Load RTMP Streams
async function loadRtmpStreams() {
    const container = document.getElementById('rtmp-content');
    container.innerHTML = '<div class="loading"><div class="spinner"></div></div>';

    try {
        const response = await fetch(`${API_BASE_URL}/rtmp/stream`);
        const data = await response.json();

        if (data.code === 0 && data.result && data.result.length > 0) {
            let html = `
                <table>
                    <thead>
                        <tr>
                            <th>Stream ID</th>
                            <th>${t('videoCodec')}</th>
                            <th>${t('audioCodec')}</th>
                            <th>${t('action')}</th>
                        </tr>
                    </thead>
                    <tbody>
            `;

            data.result.forEach(item => {
                html += `
                    <tr>
                        <td>${escapeHtml(item.stream)}</td>
                        <td>${escapeHtml(item.videoCodec || '-')}</td>
                        <td>${escapeHtml(item.audioCodec || '-')}</td>
                        <td>
                            <span class="preview-icon" onclick="playRtmpStream('${escapeHtml(item.stream)}')" title="Preview">
                                <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M14.752 11.168l-3.197-2.132A1 1 0 0010 9.87v4.263a1 1 0 001.555.832l3.197-2.132a1 1 0 000-1.664z" />
                                    <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
                                </svg>
                            </span>
                        </td>
                    </tr>
                `;
            });

            html += '</tbody></table>';
            container.innerHTML = html;
        } else {
             container.innerHTML = `
                <div class="empty-state">
                    <svg xmlns="http://www.w3.org/2000/svg" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                        <path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M5.5 16a3.5 3.5 0 01-.369-6.98 4 4 0 117.753-1.977A4.5 4.5 0 1113.5 16h-8z" />
                    </svg>
                    <p>${t('noRtmpStreamsFound')}</p>
                </div>
            `;
        }
    } catch (error) {
        container.innerHTML = `<div class="error-message">Error: ${error.message}</div>`;
    }
}

// Play RTMP Stream
async function playRtmpStream(streamId) {
    const modal = document.getElementById('video-modal');
    const videoElement = document.getElementById('video-player');
    const videoTitle = document.getElementById('video-title');
    const videoLoading = document.getElementById('video-loading');

    // Show modal and loading
    modal.classList.add('active');
    videoTitle.textContent = `Stream: ${streamId}`;
    videoLoading.style.display = 'block';

    try {
        const response = await fetch(`${API_BASE_URL}/rtmp/stream/url?stream=${encodeURIComponent(streamId)}`);
        const data = await response.json();

        if (data.code === 0 && data.result && data.result.httpFlvUrl) {
            const flvUrl = data.result.httpFlvUrl;
            
            // Destroy existing player if any
            if (mpegtsPlayer) {
                mpegtsPlayer.destroy();
                mpegtsPlayer = null;
            }

            // Check if mpegts.js is supported
            if (mpegts.isSupported()) {
                mpegtsPlayer = mpegts.createPlayer({
                    type: 'flv',
                    url: flvUrl,
                    isLive: true,
                    liveSync: true,
                    liveSyncTargetLatency: 0.5,
                    enableWorker: true
                });
                mpegtsPlayer.attachMediaElement(videoElement);
                mpegtsPlayer.load();
                mpegtsPlayer.play();
            } else {
                alert(t('browserNotSupported'));
                closeVideoModal();
            }
        } else {
            alert('Failed to get playback URL: ' + (data.msg || data.message || 'Unknown error'));
            closeVideoModal();
        }
    } catch(error) {
        alert('Error: ' + error.message);
        closeVideoModal();
    } finally {
        videoLoading.style.display = 'none';
    }
}
