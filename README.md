# Media Server

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build Base](https://github.com/greenjim301-ux/media-server/actions/workflows/build-base.yml/badge.svg)](https://github.com/greenjim301-ux/media-server/actions/workflows/build-base.yml)
[![Build with HTTPS](https://github.com/greenjim301-ux/media-server/actions/workflows/build-https.yml/badge.svg)](https://github.com/greenjim301-ux/media-server/actions/workflows/build-https.yml)
[![Build with WebRTC](https://github.com/greenjim301-ux/media-server/actions/workflows/build-rtc.yml/badge.svg)](https://github.com/greenjim301-ux/media-server/actions/workflows/build-rtc.yml)

A  media server implementation supporting GB/T 28181, RTSP, WebRTC and HTTP streaming protocols.

## Table of Contents
- [Features](#features)
- [Dependencies](#dependencies)
- [Build Instructions](#build-instructions)
- [Configuration](#configuration)
- [Usage](#usage)
- [Web Management Page](#web-management-page)
- [API Usage](#api-usage)
  - [Get Device List](#get-device-list)
  - [Add RTSP Device](#add-rtsp-device)
  - [Add ONVIF Device](#add-onvif-device)
  - [Get Device Preview URL](#get-device-preview-url)
  - [PTZ Control](#ptz-control)
  - [Query Presets](#query-presets)
  - [File Playback](#file-playback)
  - [GB28181 Integration](#gb28181-integration)
  - [GB28181 Record Playback](#gb28181-record-playback)
  - [WebRTC WHIP Usage](#webrtc-whip-usage)
  - [WebRTC WHEP Usage](#webrtc-whep-usage)

## Features

- **GB/T 28181 Support**: Implements GB/T 28181 standard for video surveillance networking systems.
- **RTSP Server**: Supports Real Time Streaming Protocol (RTSP) for media streaming.
- **HTTP Server**: Built-in HTTP server for management and signaling.
- **HTTP Streaming**: Support for media streaming over HTTP.
- **WebRTC Support**: Support for WebRTC WHIP (publish) and WHEP (playback) protocols.
- **ONVIF Support**: Includes handling for ONVIF protocol.
- **Device Management**: Manages connected devices.
- **Database Integration**: Uses SQLite for data persistence.
- **Extensible Architecture**: Built with a modular design using a reactor pattern.

## Dependencies

The project requires the following dependencies:

- **C++ Compiler**: Supports C++17 standard.
- **CMake**: Version 3.10 or higher.
- **FFmpeg**: Requires `libavcodec`, `libavformat`, `libavutil`, and `libswresample`. Recommended version 8.0 and above.
- **SQLite3**: Requires `sqlite3` library/headers installed on the system.
- **TinyXML2**: Source included.
- **OpenSSL** (Optional): Required if HTTPS support is enabled.
- **libdatachannel** (Optional): Required if WebRTC support is enabled.


## Build Instructions

1. **Create a build directory:**
   ```bash
   mkdir build
   cd build
   ```

2. **Configure with CMake:**
   ```bash
   cmake ..
   ```
   To enable HTTPS support, use:
   ```bash
   cmake -DENABLE_HTTPS=1 ..
   ```
   To enable WebRTC support, use:
   ```bash
   cmake -DENABLE_RTC=1 ..
   ```

3. **Build the project:**
   ```bash
   cmake --build .
   ```

The executable `media_server` will be generated in the `output` directory.

## Configuration

Configuration is loaded from a JSON file. The `conf` directory in the output folder typically contains `config.json`. The server sets up logging, and starts various service modules (GB, RTSP, HTTP) based on this configuration.

**Note:** You need to set \`localBindIP\` in \`config.json\` to the server's IP address before starting the service.
If HTTPS support is enabled (built with `-DENABLE_HTTPS=1`), you need to specify the SSL certificate and key paths in `config.json`:

```json
{
  "sslCertFile": "path/to/cert.pem",
  "sslKeyFile": "path/to/key.pem"
}
```
## Usage

You can use the provided scripts to start and stop the service:

```bash
cd output
# Start the service
./start.sh

# Stop the service
./stop.sh
```

Or run the compiled executable directly:

```bash
cd output
./media_server
```

Ensure the configuration file and database are accessible as expected by the application.

## Web Management Page

The media server includes a web-based management interface located in the `web` directory. This page provides a user-friendly interface for managing devices, files, GB28181 domains, and WebRTC sessions.

### Accessing the Web Page

1. Copy the `web` directory to your web server or open `web/index.html` directly in a browser.
2. Update the `API_BASE_URL` in `web/script.js` to point to your media server:
   ```javascript
   const API_BASE_URL = 'https://<server_ip>:<httpPort>';
   ```

### Features

- **Device Management**
  - View all devices with status, codec, and resolution information
  - Add new RTSP or ONVIF devices
  - Preview live streams using WebRTC (WHEP)
  - Delete devices

- **File Management**
  - View uploaded media files with metadata (size, codec, duration, etc.)
  - Upload new media files
  - Preview files using HTTP-FLV streaming
  - Delete files

- **GB28181 Management**
  - View GB domain list with device counts
  - Sync device catalog from GB28181 platforms
  - View GB server configuration
  - Search and playback GB28181 recordings (with 1-hour time range limit)

- **WebRTC Sessions**
  - View active WebRTC sessions
  - Preview streams using WHEP

### Language Support

The web page supports both English and Chinese languages. Click the **EN** or **中文** button in the sidebar to switch languages. Your preference is saved in the browser's local storage.

### Screenshot

```
+------------------+----------------------------------------+
|  Media Server    |  Device List                    [+] [↻] |
|  [EN] [中文]     |----------------------------------------|
|                  |  Device ID | Name | Protocol | Status  |
|  ▶ Device        |  --------- | ---- | -------- | ------  |
|  📄 File         |  cam_001   | Cam1 | RTSP     | ON      |
|  🏢 GB Domain    |  cam_002   | Cam2 | GB28181  | ON      |
|  📡 WebRTC       |                                        |
+------------------+----------------------------------------+
```

## API Usage


### Get Device List

To retrieve the list of devices or a specific device, send a HTTP GET request to `/device`.

**URL:** `http://<server_ip>:<httpPort>/device`

**Method:** `GET`

**Parameters:**

- `deviceId` (optional): The ID of the specific device to retrieve. If omitted, all devices are returned.

**Example using `curl`:**

**Get all devices:**
```bash
curl -X GET http://127.0.0.1:26080/device
```

**Get a specific device:**
```bash
curl -X GET "http://127.0.0.1:26080/device?deviceId=rtsp_cam_01"
```

**Response Example:**
```json
{
  "code": 0,
  "msg": "success",
  "result": [
    {
      "deviceId": "rtsp_cam_01",
      "domainId": "domain_01",
      "name": "Camera 01",
      "type": "camera",
      "parentId": ["parent_01"],
      "status": "online",
      "manufacturer": "Hikvision",
      "model": "DS-2CD2T45",
      "owner": "admin",
      "civilCode": "110000",
      "address": "Building A",
      "ipAddr": "192.168.1.100",
      "user": "admin",
      "pass": "******",
      "port": 554,
      "longitude": "116.397128",
      "latitude": "39.916527",
      "ptzType": 1,
      "url": "rtsp://192.168.1.100:554/stream1",
      "protocol": 0,
      "codec": "H.264",
      "resolution": "1920x1080",
      "bindIP": "0.0.0.0",
      "remark": "Front entrance camera",
      "onvifProfile": "Profile_1",
      "onvifPtzUrl": "http://192.168.1.100/onvif/ptz"
    }
  ]
}
```

### Add RTSP Device

To add an RTSP device, send a HTTP POST request to `/device` with the following JSON body:

**URL:** `http://<server_ip>:<httpPort>/device`

**Method:** `POST`

**Body:**

```json
{
    "name": "My_RTSP_Camera",
    "protocol": 2,
    "url": "rtsp://admin:123456@192.168.1.100:554/ch1/main/av_stream"
}
```

- `name`: Display name for the device (string).
- `protocol`: `2` for RTSP device.
- `url`: The full RTSP stream URL.

**Example using `curl`:**

```bash
curl -X POST http://127.0.0.1:26080/device \
  -H "Content-Type: application/json" \
  -d '{
        "name": "Door Camera",
        "protocol": 2,
        "url": "rtsp://192.168.1.50:554/live"
      }'
```

### Add ONVIF Device

To add an ONVIF device, send a HTTP POST request to `/device` with the following JSON body. The media server will probe the ONVIF device, get the RTSP URL and PTZ control URL, and add this device as an RTSP device.

**URL:** `http://<server_ip>:<httpPort>/device`

**Method:** `POST`

**Body:**

```json
{
    "name": "My_ONVIF_Camera",
    "protocol": 4,
    "ipAddr": "192.168.1.100",
    "user": "admin",
    "pass": "123456"
}
```

- `name`: Display name for the device (string).
- `protocol`: `4` for ONVIF device.
- `ipAddr`: The IP address of the ONVIF device.
- `user`: ONVIF username.
- `pass`: ONVIF password.

**Example using `curl`:**

```bash
curl -X POST http://127.0.0.1:26080/device \
  -H "Content-Type: application/json" \
  -d '{
        "name": "Office Camera",
        "protocol": 4,
        "ipAddr": "192.168.1.100",
        "user": "admin",
        "pass": "123456"
      }'
```

### Get Device Preview URL

To retrieve the live streaming URLs (RTSP, HTTP-TS, HTTP-FLV) for a device.

**URL:** `http://<server_ip>:<httpPort>/device/url`

**Method:** `GET` or `POST`

**Parameters:**

- `deviceId` (required): The ID of the device.
- `netType` (optional): Network type preference.

**Example using `curl`:**

```bash
curl -X POST http://127.0.0.1:26080/device/url \
  -H "Content-Type: application/json" \
  -d '{
        "deviceId": "rtsp_cam_01"
      }'
```

**Response:**

```json
{
    "code": 0,
    "msg": "success",
    "result": {
        "rtspUrl": "rtsp://192.168.1.100:554/live/rtsp_cam_01",
        "httpTsUrl": "http://192.168.1.100:8080/live/rtsp_cam_01.ts",
        "httpFlvUrl": "http://192.168.1.100:8080/live/rtsp_cam_01.flv",
        "rtcUrl": "http://192.168.1.100:8080/rtc/whep/rtsp_cam_01"
    }
}
```

**Note:** You can use [mpegts.js](https://github.com/xqq/mpegts.js) to play HTTP-FLV/TS streams in the browser.

**Codec Support:** Media-server only supports H.264, H.265, AAC, and Opus codecs. The `rtcUrl` is for WebRTC WHEP playback. For WHEP, only H.264, H.265, and Opus are supported. AAC audio will be transcoded to Opus automatically.

### PTZ Control

To control the PTZ (Pan-Tilt-Zoom) of a device. Supported for GB28181 and ONVIF devices. Note that some devices may not support all commands.

**URL:** `http://<server_ip>:<httpPort>/device/ptz`

**Method:** `POST`

**Body:**

```json
{
    "deviceId": "34020000001320000001",
    "ptzCmd": 1,
    "timeout": 500,
    "presetID": "1"
}
```

- `deviceId` (required): The ID of the device.
- `ptzCmd` (required): The PTZ command integer.
  - `1`: Move Left
  - `2`: Move Right
  - `3`: Move Up
  - `4`: Move Down
  - `5`: Zoom In
  - `6`: Zoom Out
  - `7`: Goto Preset
  - `8`: Set Preset
  - `9`: Delete Preset
  - `11`: Move Left Up
  - `12`: Move Right Up
  - `13`: Move Left Down
  - `14`: Move Right Down
- `timeout` (optional): The duration of the movement in milliseconds. After this time, the movement stops (default: 500).
- `presetID` (optional): The preset ID, required for preset commands (7, 8, 9).

### Query Presets

To query the presets of a device.

**URL:** `http://<server_ip>:<httpPort>/device/preset`

**Method:** `GET` or `POST`

**GET Example:**

```bash
curl -X GET "http://127.0.0.1:26080/device/preset?deviceId=34020000001320000001"
```

**POST Body:**

```json
{
    "deviceId": "34020000001320000001"
}
```

- `deviceId` (required): The ID of the device.

**Response Example:**

```json
{
    "code": 0,
    "msg": "ok",
    "result": [
        {"presetID": "1"},
        {"presetID": "2"},
        {"presetID": "3"}
    ]
}
```

### File Playback

1. **Upload File**

   Upload a video file to the server using HTTP multipart/form-data.

   **URL:** `http://<server_ip>:<httpPort>/file/upload`
   **Method:** `POST`
   **Content-Type:** `multipart/form-data`

2. **Get File List**

   Retrieve the list of uploaded files or information about a specific file.

   **URL:** `http://<server_ip>:<httpPort>/file`
   **Method:** `GET`
   **Parameters:**
   - `fileId` (optional): The ID of the specific file to retrieve. If omitted, all files are returned.

   **Response Example:**
   ```json
   {
       "code": 0,
       "msg": "success",
       "result": [
           {
               "fileId": 1,
               "fileName": "sample.mp4",
               "size": 10485760,
               "codec": "h264",
               "resolution": "1920x1080",
               "duration": 120.5,
               "frameRate": 25.0
           }
       ]
   }
   ```

3. **Get File Playback URL**

   Get the playback URL for a specific file.

   **URL:** `http://<server_ip>:<httpPort>/file/url`
   **Method:** `GET`
   **Parameters:**
   - `fileId` (required): The ID of the file.
   - `netType` (optional): Network type preference.

   **Response Example:**
   ```json
   {
       "code": 0,
       "msg": "success",
       "result": {
           "rtspUrl": "rtsp://192.168.1.100:554/vod/abc123xyz/sample.mp4",
           "httpTsUrl": "http://192.168.1.100:8080/vod/abc123xyz/ts/sample.mp4",
           "httpFlvUrl": "http://192.168.1.100:8080/vod/abc123xyz/flv/sample.mp4"
       }
   }
   ```

### GB28181 Integration

1. **Get GB Server Information**

   Retrieve the server information required to configure your GB28181 device or platform.

   **URL:** `http://<server_ip>:<httpPort>/gb/server`
   **Method:** `GET`

   **Example:**
   ```bash
   curl -X GET http://127.0.0.1:26080/gb/server
   ```

   **Response:**
   ```json
   {
       "code": 0,
       "msg": "success",
       "result": {
           "id": "34020000002000000001",
           "ip": "192.168.1.100",
           "port": 5080,
           "pass": "12345678",
           "rtpTransport": 2
       }
   }
   ```

   **Note on `rtpTransport`:**
   The `rtpTransport` field indicates the transport method the GB server uses to receive streams from the device or platform:
   - `0`: UDP
   - `1`: TCP Active
   - `2`: TCP Passive (Default)

   You can change this value by modifying the `rtpTransport` field in `conf/config.json`.

2. **Configure GB Device/Platform**

   Use the information from the previous step (`id`, `ip`, `port`, `pass`) to configure the "SIP Server" or "Platform Access" settings on your GB28181 device or platform.

3. **Verify Registration**

   After configuration, the device should register automatically. Check the registered domains/devices.

   **URL:** `http://<server_ip>:<httpPort>/gb/domain`
   **Method:** `GET`

   **Example:**
   ```bash
   curl -X GET http://127.0.0.1:26080/gb/domain
   ```

   **Response Example:**
   ```json
   {
       "code": 0,
       "msg": "success",
       "result": [
           {
               "id": "34020000002000000002",
               "devNum": 5,
               "ip": "192.168.1.50",
               "port": 5060
           }
       ]
   }
   ```

4. **Sync Device Channel List**

   The server usually syncs the device list automatically upon registration. If this does not happen, you can trigger it manually.

   **URL:** `http://<server_ip>:<httpPort>/gb/catalog`
   **Method:** `GET`

   **Example:**
   ```bash
   curl -X GET http://127.0.0.1:26080/gb/catalog
   ```

   Once the device channels are synced, you can use the [Get Device List](#get-device-list) and [Get Device Preview URL](#get-device-preview-url) APIs to access the video streams.

### GB28181 Record Playback

1. **Query Record List**

   To query the recording files on a device, send a POST request to `/gb/record`.

   **URL:** `http://<server_ip>:<httpPort>/gb/record`
   **Method:** `POST`

   **Body:**
   ```json
   {
       "deviceId": "34020000001320000001",
       "startTime": "2023-10-27T10:00:00",
       "endTime": "2023-10-27T11:00:00",
       "type": "all"
   }
   ```

   - `deviceId`: GB device ID (must be sub-device/channel ID)
   - `startTime`: Start time (ISO 8601 format or similar string)
   - `endTime`: End time
   - `type`: Record type ("all", etc.)

   **Response Example:**
   ```json
   {
       "code": 0,
       "msg": "success",
       "result": [
           {
               "deviceId": "34020000001320000001",
               "name": "Recording 1",
               "startTime": "2023-10-27T10:00:00",
               "endTime": "2023-10-27T10:30:00",
               "type": "time"
           },
           {
               "deviceId": "34020000001320000001",
               "name": "Recording 2",
               "startTime": "2023-10-27T10:30:00",
               "endTime": "2023-10-27T11:00:00",
               "type": "time"
           }
       ]
   }
   ```

2. **Get Playback URL**

   After identifying the recording you want to play, request the playback stream URL.

   **URL:** `http://<server_ip>:<httpPort>/gb/record/url`
   **Method:** `POST`

   **Body:**
   ```json
   {
       "deviceId": "34020000001320000001",
       "startTime": "2023-10-27T10:00:00",
       "endTime": "2023-10-27T11:00:00",
       "type": "time"
   }
   ```

   **Response:**
   ```json
   {
       "code": 0,
       "msg": "success",
       "result": {
           "rtspUrl": "rtsp://192.168.1.100:554/gbvod/randStr/34020000001320000001-start-end",
           "httpTsUrl": "http://192.168.1.100:8080/gbvod/randStr/34020000001320000001-start-end.ts",
           "httpFlvUrl": "http://192.168.1.100:8080/gbvod/randStr/34020000001320000001-start-end.flv"
       }
   }
   ```

   You can use RTSP or HTTP-FLV/TS players (like mpegts.js) to play these playback streams.

### WebRTC WHIP Usage

1. **Publish Stream (WHIP)**

   Publish a WebRTC stream to the media server using the WHIP protocol.

   **URL:** `http://<server_ip>:<httpPort>/rtc/whip`
   **Method:** `POST`
   **Body:** SDP Offer

   **Response:** SDP Answer (201 Created)

   **Codec Support:** For WHIP, only H.264, H.265, and Opus codecs are supported.

2. **Get Live Sessions**

   Retrieve the list of active WebRTC sessions.

   **URL:** `http://<server_ip>:<httpPort>/rtc/session`
   **Method:** `GET`

   **Response Example:**
   ```json
   {
       "code": 0,
       "msg": "ok",
       "result": [
           {
               "sessionId": "abc123xyz456",
               "videoCodec": "H264",
               "audioCodec": "OPUS"
           }
       ]
   }
   ```

3. **Get Playback URL**

   Get the playback URL for a specific WebRTC session.

   **URL:** `http://<server_ip>:<httpPort>/rtc/session/url`
   **Method:** `GET`
   **Parameters:**
   - `sessionId` (required): The ID of the session.

   **Response Example:**
   ```json
   {
     "code": 0,
     "msg": "success",
     "result": {
       "httpFlvUrl": "http://192.168.1.100:8080/live/abc123.flv",
       "httpTsUrl": "http://192.168.1.100:8080/live/abc123.ts",
       "rtcUrl": "http://192.168.1.100:8080/rtc/whep/abc123",
       "rtspUrl": "rtsp://192.168.1.100:554/live/abc123"
     }
   }
   ```

   **Note:** Converting WebRTC streams to HTTP-FLV requires FFmpeg 8.0 or above.

### WebRTC WHEP Usage

WHEP (WebRTC-HTTP Egress Protocol) allows you to play live streams via WebRTC with ultra-low latency.

**Supported Sources:**

1. **RTSP and GB28181 Live Cameras**

   For RTSP cameras and GB28181 devices, use the `rtcUrl` returned by `GET /device/url`:

   ```
   GET /device/url?deviceId=<deviceId>
   ```

   The response `rtcUrl` field (e.g., `http://192.168.1.100:8080/rtc/whep/rtsp_cam_01`) can be used directly with a WHEP client.

2. **WHIP Streams**

   For WebRTC streams published via WHIP, use the `rtcUrl` returned by `GET /rtc/session/url`:

   ```
   GET /rtc/session/url?sessionId=<sessionId>
   ```

   The response `rtcUrl` field (e.g., `http://192.168.1.100:8080/rtc/whep/abc123`) can be used directly with a WHEP client.

**WHEP Playback:**

   **URL:** `http://<server_ip>:<httpPort>/rtc/whep/<streamId>`
   **Method:** `POST`
   **Body:** SDP Offer

   **Response:** SDP Answer (201 Created)

   **Codec Support:** For WHEP, only H.264, H.265, and Opus codecs are supported. AAC audio will be transcoded to Opus automatically.
