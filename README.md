# 🎵 歌词小舟 dock-lyrics

> 一个 Deepin (dde-shell) 任务栏插件：检测到音乐开始播放时自动"唤醒"，
> 在任务栏上以跑马灯形式滚动显示当前歌词。

![dock-lyrics](docs/screenshot-main.png)

## 功能

- **播放唤醒**：任务栏监听 MPRIS（`org.mpris.MediaPlayer2.*`），任一播放器开始播放即自动出现歌词条；暂停后自动收起，不打扰桌面。
- **跑马灯歌词**：随歌曲进度同步滚动显示当前行，超长歌词自动平滑滚动，深色/浅色任务栏都清晰可读。
- **双路歌词来源**：优先读取本地 `.lrc`（与音乐同目录的 sidecar 文件，或 `~/Music`、`~/音乐` 常见目录），找不到再联网到网易云音乐曲库检索，失败时优雅降级为显示「歌名 - 歌手」。
- **动画均衡器**：左侧 3 条动感音柱随播放跳动。
- **点击控制**：单击歌词条即播放/暂停；悬停弹出气泡显示 歌名 / 歌手 / 当前句 与来源状态。
- **适配任意任务栏位置**：任务栏在屏幕上下时显示完整歌词跑马灯；在左右（竖排）时自动收起为均衡器图标。

## 编译安装

依赖：`dde-shell` 开发包（`libdde-shell-dev`、`libdde-shell-dock-dev`）、Qt 6（Core/Gui/Network/DBus）、CMake ≥ 3.16。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
sudo cmake --install build          # 安装到系统插件目录
systemctl --user restart dde-shell@DDE.service
```

也可以直接打 deb 安装：

```bash
bash deb/build-deb.sh
sudo dpkg -i dock-lyrics_1.0.0_amd64.deb
```

## 使用

1. 安装插件并重启 dde-shell。
2. 用任意支持 MPRIS 的音乐播放器播放音乐（如 Deepin 音乐、网易云音乐、VLC、Spotify…）。
3. 任务栏即出现歌词条，随播放进度滚动显示当前歌词。

> 想体验但手边没有播放器？可运行仓库内脚本
> `python3 /tmp/mpris_mock.py`（一个模拟 MPRIS 播放器）来试玩。

## 歌词来源优先级

1. 音乐文件旁的同名 `.lrc`（例如 `晴天.mp3` ↔ `晴天.lrc`）。
2. `~/Music`、`~/音乐`、`~/Music/Lyrics`、`~/音乐/歌词` 目录中的常见命名（`歌名.lrc`、`歌手 - 歌名.lrc`…）。
3. 网易云音乐在线检索（`music.163.com/api`，取最佳匹配后拉取逐字歌词）。

## 工程结构

```
CMakeLists.txt           构建脚本（ds_install_package）
package/
  metadata.json          插件元数据（Id / Parent: org.deepin.ds.dock）
  main.qml               任务栏组件 UI（跑马灯 + 均衡器 + 交互）
src/
  lyricsapplet.*         DApplet 后端：MPRIS 监听、活动播放器决策、歌词状态机
  playerprobe.*          逐播放器 PropertiesChanged 探测
  lyricsfetcher.*        本地 lrc 查找 + 网易云在线获取
  lrcparser.*            LRC 时间轴解析/元数据行过滤
deb/                     打包脚本
```

## 许可

GPL-3.0-or-later
