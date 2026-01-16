# 媒体服务器 (Media Server)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build Base](https://github.com/greenjim301-ux/media-server/actions/workflows/build-base.yml/badge.svg)](https://github.com/greenjim301-ux/media-server/actions/workflows/build-base.yml)
[![Build with HTTPS](https://github.com/greenjim301-ux/media-server/actions/workflows/build-https.yml/badge.svg)](https://github.com/greenjim301-ux/media-server/actions/workflows/build-https.yml)
[![Build with WebRTC](https://github.com/greenjim301-ux/media-server/actions/workflows/build-rtc.yml/badge.svg)](https://github.com/greenjim301-ux/media-server/actions/workflows/build-rtc.yml)

一个支持 GB/T 28181、RTSP、WebRTC 和 HTTP 流媒体协议的媒体服务器实现。

## 目录
- [功能特性](#功能特性)
- [依赖项](#依赖项)
- [构建指南](#构建指南)
- [配置](#配置)
- [使用方法](#使用方法)
- [API 使用](#api-使用)
  - [获取设备列表](#获取设备列表)
  - [添加 RTSP 设备](#添加-rtsp-设备)
  - [添加 ONVIF 设备](#添加-onvif-设备)
  - [获取设备预览地址](#获取设备预览地址)
  - [PTZ 控制](#ptz-控制)
  - [查询预置位](#查询预置位)
  - [文件回放](#文件回放)
  - [GB28181 集成](#gb28181-集成)
  - [GB28181 录像回放](#gb28181-录像回放)
  - [WebRTC WHIP 使用](#webrtc-whip-使用)

## 功能特性

- **GB/T 28181 支持**: 实现视频监控联网系统的 GB/T 28181 标准。
- **RTSP 服务器**: 支持实时流传输协议 (RTSP) 进行媒体流分发。
- **HTTP 服务器**: 内置 HTTP 服务器，用于管理和信令交互。
- **HTTP 流媒体**: 支持通过 HTTP 协议传输媒体流。
- **WebRTC WHIP 支持**: 支持 WebRTC WHIP 协议。
- **ONVIF 支持**: 包含对 ONVIF 协议的处理。
- **设备管理**: 管理连接的设备。
- **数据库集成**: 使用 SQLite 进行数据持久化。
- **可扩展架构**: 采用基于 reactor 模式的模块化设计。

## 依赖项

通过包管理器或源码安装以下依赖：

- **C++ 编译器**: 支持 C++17 标准。
- **CMake**: 版本 3.10 或更高。
- **FFmpeg**: 需要 `libavcodec`, `libavformat`, `libavutil` 和 `libswresample` 库。推荐版本 8.0 及以上。
- **SQLite3**: 需在系统中安装 `sqlite3` 库和头文件。
- **TinyXML2**: 已包含源码。
- **OpenSSL** (可选): 仅在启用 HTTPS 支持时需要。
- **libdatachannel** (可选): 仅在启用 WebRTC 支持时需要。

## 构建指南

1. **创建构建目录:**
   ```bash
   mkdir build
   cd build
   ```

2. **配置 CMake:**
   ```bash
   cmake ..
   ```
   若要启用 HTTPS 支持，请使用:
   ```bash
   cmake -DENABLE_HTTPS=1 ..
   ```
   若要启用 WebRTC 支持，请使用:
   ```bash
   cmake -DENABLE_RTC=1 ..
   ```

3. **编译项目:**
   ```bash
   cmake --build .
   ```

可执行文件 `media_server` 将生成在 `output` 目录中。

## 配置

配置信息从 JSON 文件加载。输出目录中的 `conf` 文件夹通常包含 `config.json`。服务器将根据此配置设置日志记录、并启动各种服务模块（GB, RTSP, HTTP）。

**注意:** 在启动服务之前，您需要在 `config.json` 中将 `localBindIP` 设置为服务器的 IP 地址。

如果启用了 HTTPS 支持（使用 `-DENABLE_HTTPS=1` 构建），则需要在 `config.json` 中指定 SSL 证书和密钥路径：

```json
{
  "sslCertFile": "path/to/cert.pem",
  "sslKeyFile": "path/to/key.pem"
}
```

## 使用方法

你可以使用提供的脚本来启动和停止服务：

```bash
cd output
# 启动服务
./start.sh

# 停止服务
./stop.sh
```

或者直接运行编译后的可执行文件：

```bash
cd output
./media_server
```

请确保配置文件和数据库可被应用程序正确访问。

## API 使用

### 获取设备列表

要获取设备列表或特定设备，请发送 HTTP GET 请求到 `/device`。

**URL:** `http://<server_ip>:<httpPort>/device`

**方法:** `GET`

**参数:**

- `deviceId` (可选): 要获取的特定设备的 ID。如果省略，将返回所有设备。

**使用 `curl` 的示例:**

**获取所有设备:**
```bash
curl -X GET http://127.0.0.1:26080/device
```

**获取特定设备:**
```bash
curl -X GET "http://127.0.0.1:26080/device?deviceId=rtsp_cam_01"
```

### 添加 RTSP 设备

要添加 RTSP 设备，请发送 HTTP POST 请求到 `/device`，并在 Body 中包含以下 JSON 数据：

**URL:** `http://<server_ip>:<httpPort>/device`

**方法:** `POST`

**Body:**

```json
{
    "name": "My_RTSP_Camera",
    "protocol": 2,
    "url": "rtsp://admin:123456@192.168.1.100:554/ch1/main/av_stream"
}
```

- `name`: 设备的显示名称 (字符串)。
- `protocol`: `2` 代表 RTSP 设备。
- `url`: 完整的 RTSP 流地址。

**使用 `curl` 的示例:**

```bash
curl -X POST http://127.0.0.1:26080/device \
  -H "Content-Type: application/json" \
  -d '{
        "name": "Door Camera",
        "protocol": 2,
        "url": "rtsp://192.168.1.50:554/live"
      }'
```

### 添加 ONVIF 设备

要添加 ONVIF 设备，请发送 HTTP POST 请求到 `/device`，并在 Body 中包含以下 JSON 数据。媒体服务器将探测 ONVIF 设备，获取 RTSP URL 和 PTZ 控制 URL，并将此设备添加为 RTSP 设备。

**URL:** `http://<server_ip>:<httpPort>/device`

**方法:** `POST`

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

- `name`: 设备的显示名称 (字符串)。
- `protocol`: `4` 代表 ONVIF 设备。
- `ipAddr`: ONVIF 设备的 IP 地址。
- `user`: ONVIF 用户名。
- `pass`: ONVIF 密码。

**使用 `curl` 的示例:**

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

### 获取设备预览地址

获取设备的实时流地址 (RTSP, HTTP-TS, HTTP-FLV)。

**URL:** `http://<server_ip>:<httpPort>/device/url`

**方法:** `GET` 或 `POST`

**参数:**

- `deviceId` (必填): 设备 ID。
- `netType` (可选): 网络类型偏好。

**使用 `curl` 的示例:**

```bash
curl -X POST http://127.0.0.1:26080/device/url \
  -H "Content-Type: application/json" \
  -d '{
        "deviceId": "rtsp_cam_01"
      }'
```

**响应:**

```json
{
    "code": 0,
    "msg": "success",
    "result": {
        "rtspUrl": "rtsp://192.168.1.100:554/live/rtsp_cam_01",
        "httpTsUrl": "http://192.168.1.100:8080/live/rtsp_cam_01.ts",
        "httpFlvUrl": "http://192.168.1.100:8080/live/rtsp_cam_01.flv"
    }
}
```

**注意:** 您可以使用 [mpegts.js](https://github.com/xqq/mpegts.js) 在浏览器中播放 HTTP-FLV/TS 流。

### PTZ 控制

控制设备的云台 (Pan-Tilt-Zoom)。支持 GB28181 和 ONVIF 设备。注意：部分设备可能不支持所有指令。

**URL:** `http://<server_ip>:<httpPort>/device/ptz`

**方法:** `POST`

**Body:**

```json
{
    "deviceId": "34020000001320000001",
    "ptzCmd": 1,
    "timeout": 500,
    "presetID": "1"
}
```

- `deviceId` (必填): 设备 ID。
- `ptzCmd` (必填): PTZ 控制指令 (整数)。
  - `1`: 左移
  - `2`: 右移
  - `3`: 上移
  - `4`: 下移
  - `5`: 放大
  - `6`: 缩小
  - `7`: 调用预置位
  - `8`: 设置预置位
  - `9`: 删除预置位
  - `11`: 左上移
  - `12`: 右上移
  - `13`: 左下移
  - `14`: 右下移
- `timeout` (可选): 云台转动的持续时间（毫秒），超过该时间后停止转动 (默认: 500)。
- `presetID` (可选): 预置位 ID，预置位相关指令 (7, 8, 9) 必填。

### 查询预置位

查询设备的预置位列表。

**URL:** `http://<server_ip>:<httpPort>/device/preset`

**方法:** `POST`

**Body:**

```json
{
    "deviceId": "34020000001320000001"
}
```

- `deviceId` (必填): 设备 ID。

### 文件回放

1. **上传文件**

   使用 HTTP multipart/form-data 将视频文件上传到服务器。

   **URL:** `http://<server_ip>:<httpPort>/file/upload`
   **方法:** `POST`
   **Content-Type:** `multipart/form-data`

2. **获取文件列表**

   获取已上传文件的列表或特定文件的信息。

   **URL:** `http://<server_ip>:<httpPort>/file`
   **方法:** `GET`
   **参数:**
   - `fileId` (可选): 特定文件的文件 ID。如果省略，将返回所有文件。

3. **获取文件播放地址**

   获取特定文件的播放地址。

   **URL:** `http://<server_ip>:<httpPort>/file/url`
   **方法:** `GET`
   **参数:**
   - `fileId` (必填): 文件 ID。

### GB28181 集成

1. **获取 GB 服务器信息**

   获取配置 GB28181 设备或平台所需的服务器信息。

   **URL:** `http://<server_ip>:<httpPort>/gb/server`
   **方法:** `GET`

   **示例:**
   ```bash
   curl -X GET http://127.0.0.1:26080/gb/server
   ```

   **响应:**
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

   **关于 `rtpTransport` 的说明:**
   `rtpTransport` 字段指示 GB 服务器用于接收设备或平台流的传输方式：
   - `0`: UDP
   - `1`: TCP 主动 (Active)
   - `2`: TCP 被动 (Passive) (默认)

   您可以通过修改 `conf/config.json` 中的 `rtpTransport` 字段来更改此值。

2. **配置 GB 设备/平台**

   使用上一步获取的信息 (`id`, `ip`, `port`, `pass`) 在您的 GB28181 设备或平台上配置 "SIP 服务器" 或 "平台接入" 设置。

3. **验证注册**

   配置完成后，设备应会自动注册。检查已注册的域/设备。

   **URL:** `http://<server_ip>:<httpPort>/gb/domain`
   **方法:** `GET`

   **示例:**
   ```bash
   curl -X GET http://127.0.0.1:26080/gb/domain
   ```

4. **同步设备通道列表**

   服务器通常在注册时自动同步设备列表。如果未同步，您可以手动触发。

   **URL:** `http://<server_ip>:<httpPort>/gb/catalog`
   **方法:** `GET`

   **示例:**
   ```bash
   curl -X GET http://127.0.0.1:26080/gb/catalog
   ```

   设备通道同步后，您可以使用 [获取设备列表](#获取设备列表) 和 [获取设备预览地址](#获取设备预览地址) API 来访问视频流。

### GB28181 录像回放

1. **查询录像列表**

   要查询设备上的录像文件，请发送 POST 请求到 `/gb/record`。

   **URL:** `http://<server_ip>:<httpPort>/gb/record`
   **方法:** `POST`

   **Body:**
   ```json
   {
       "deviceId": "34020000001320000001",
       "startTime": "2023-10-27T10:00:00",
       "endTime": "2023-10-27T11:00:00",
       "type": "all"
   }
   ```

   - `deviceId`: GB 设备 ID (必须是子设备/通道 ID)
   - `startTime`: 开始时间 (ISO 8601 格式或类似字符串)
   - `endTime`: 结束时间
   - `type`: 录像类型 ("all" 等)

  
2. **获取回放地址**

   找到要播放的录像后，请求回放流地址。

   **URL:** `http://<server_ip>:<httpPort>/gb/record/url`
   **方法:** `POST`

   **Body:**
   ```json
   {
       "deviceId": "34020000001320000001",
       "startTime": "2023-10-27T10:00:00",
       "endTime": "2023-10-27T11:00:00",
       "type": "time"
   }
   ```

   **响应:**
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

   您可以使用 RTSP 或 HTTP-FLV/TS 播放器 (如 mpegts.js) 播放这些回放流。

### WebRTC WHIP 使用

1. **推流 (WHIP)**

   使用 WHIP 协议发布 WebRTC 流到媒体服务器。

   **URL:** `http://<server_ip>:<httpPort>/rtc/whip`
   **Method:** `POST`
   **Body:** SDP Offer

   **响应:** SDP Answer (201 Created)

2. **获取实时会话**

   获取当前活跃的 WebRTC 会话列表。

   **URL:** `http://<server_ip>:<httpPort>/rtc/session`
   **Method:** `GET`

3. **获取回放地址**

   获取特定 WebRTC 会话的回放地址。

   **URL:** `http://<server_ip>:<httpPort>/rtc/session/url`
   **Method:** `GET`
   **参数:**
   - `sessionId` (必填): 会话 ID。

   **注意:** 将 WebRTC 流转换为 HTTP-FLV 需要 FFmpeg 8.0 或更高版本。
