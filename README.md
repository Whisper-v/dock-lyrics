# 🎵 歌词小舟 dock-lyrics

> 一个运行在 **Deepin (dde-shell)** 任务栏上的歌词插件：检测到音乐开始播放时自动唤醒，
> 在任务栏以跑马灯形式同步滚动显示当前歌词；暂停即安静地收起，不打扰桌面。

![整条任务栏效果](docs/screenshot-main.png)

![任务栏歌词特写](docs/screenshot-closeup.png)

## ✨ 功能特性

- **播放唤醒**：监听 MPRIS（`org.mpris.MediaPlayer2.*`），任一兼容播放器开始播放即自动出现歌词条；暂停/停止后自动收起。
- **同步歌词跑马灯**：按歌曲进度实时切换当前行，超长歌词平滑滚动；深色/浅色任务栏与深浅主题均清晰可读。
- **三级歌词来源**：
  1. 本地 `.lrc`（音乐文件旁的 sidecar，或 `~/Music`、`~/音乐` 等目录）；
  2. 网易云音乐曲库（`music.163.com`，带匹配校验，避免串词）；
  3. **LRCLIB 开放歌词库**（`lrclib.net`）兜底——网易云没有版权的歌曲（如周杰伦等）也能命中。
- **动画均衡器**：歌词条左侧 3 根音柱随播放律动。
- **点击控制 / 悬停详情**：单击歌词条播放⇄暂停；悬停气泡显示 歌名 / 歌手 / 当前句 / 歌词来源（本地歌词或在线歌词）。
- **🎨 颜色主题**：11 套预设 —— **跟随系统** + 6 套高亮彩色（柠檬黄 / 苹果青 / 冰川蓝 / 霓虹紫 / 樱花粉 / 暖阳橙）+
  **4 套深色主题（曜石黑 / 深空蓝 / 暮光紫 / 墨夜绿）**：深色主题自带深色半透明胶囊底，
  在浅色任务栏上也清晰可读；另有 **自定义配色**，可分别设置**字体颜色**与**背景颜色**
  （各十余种色板，支持带透明度的深/浅背景）。文字与均衡器音柱同步换色，选择持久化到 `~/.config/deepin/dock-lyrics.conf`。
- **🖱️ 右键菜单**：歌词条上右键即可 **播放/暂停、上一曲、下一曲、颜色主题、刷新歌词**，
  并实时显示当前歌词来源（本地文件 / 网易云音乐 / LRCLIB）。
- **任意位置任务栏**：任务栏在屏幕上下方时显示完整跑马灯歌词；在左右两侧（竖排）时自动收起为均衡器图标。
- **真实播放器可用**：已实测 **QQ 音乐（桌面版/Chromium 内核）**、Deepin 音乐、VLC 等 MPRIS 播放器与本地模拟器。

## 📷 效果预览

任务栏右侧托盘区显示的实时歌词（本地歌词 / 网易云 / LRCLIB 三种来源自动切换）：

![dock-lyrics 运行效果](docs/dock-lyrics-preview.png)

## 🚀 安装

### 方式一：安装 .deb（推荐）

仓库提供打包脚本，构建并安装：

```bash
bash deb/build-deb.sh                 # 生成 dock-lyrics_1.0.0_amd64.deb
sudo dpkg -i dock-lyrics_1.0.0_amd64.deb
```

`postinst` 会自动重启当前用户的 `dde-shell@DDE.service`，安装后无需手动操作即可看到任务栏歌词。

### 方式二：源码安装

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
sudo cmake --install build            # 安装到系统 dde-shell 插件目录
systemctl --user restart dde-shell@DDE.service
```

### 卸载

```bash
sudo dpkg -r dock-lyrics              # .deb 方式卸载
systemctl --user restart dde-shell@DDE.service
```

## 🎮 使用

1. 安装插件并重启 dde-shell（安装脚本会自动处理）。
2. 用任意支持 MPRIS 的播放器播放音乐（QQ 音乐、Deepin 音乐、网易云音乐、VLC、Spotify…）。
3. 任务栏右侧即出现歌词条，随播放进度滚动显示当前歌词。
4. **右键歌词条**可唤出快捷菜单：切换播放状态、上下曲、更换**颜色主题**（跟随系统 + 6 套高亮彩色 + 4 套深色主题 + **自定义配色**
   共 12 个配色选项，即时生效并记忆）、或手动**刷新歌词**；菜单内还会标注当前歌词来自本地文件、网易云还是 LRCLIB。

> 想先体验但没有播放器？运行仓库自带的 MPRIS 模拟器即可：
>
> ```bash
> python3 tools/mpris_mock.py
> ```
>
> 模拟器会在用户总线注册 `org.mpris.MediaPlayer2.docklyricsmock`，自动“播放”《晴天》
> （配合 `~/Music/晴天.lrc` 走本地歌词，或 `next` 切到《稻香》走在线歌词）。

## 🎼 歌词来源与优先级

| 优先级 | 来源 | 说明 |
| --- | --- | --- |
| 1 | **本地 `.lrc`** | ① 正在播放的本地文件旁同名 `.lrc`；② `~/Music`、`~/音乐`、`~/Music/Lyrics`、`~/音乐/歌词` 下常见命名（`歌名.lrc`、`歌手 - 歌名.lrc` 等） |
| 2 | **网易云音乐** | 搜索命中并做“标题精确 + 歌手匹配”校验，**只取高置信度结果**，避免串到翻唱/无关歌曲 |
| 3 | **LRCLIB** | 网易云检索失败或歌词为空时自动降级；优先取带时间轴的 `syncedLyrics`，否则取纯文本逐行翻页显示 |

所有来源都找不到时，歌词条会退化为显示「歌名 - 歌手」并提示“没有找到歌词”。
**此时暂停再继续播放即可自动重试**（或重启 dde-shell 后切歌）。

> 小贴士：为本地歌曲放一个同名的 `.lrc` 即可 100% 命中本地歌词（无需联网）。

## 🛠️ 从源码构建

### 依赖

- Deepin 25 / UOS，dde-shell ≥ 2.0（插件框架）
- 开发包：`libdde-shell-dev`、`libdde-shell-dock-dev`
- Qt 6：Core / Gui / Network / DBus
- CMake ≥ 3.16，C++17 编译器

```bash
sudo apt install cmake g++ qt6-base-dev libqt6network6 libqt6dbus6 \
     libdde-shell-dev libdde-shell-dock-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

产物：

- `build/plugins/org.deepin.ds.dock.lyrics.so` — C++ 后端插件
- `build/packages/org.deepin.ds.dock.lyrics/` — QML 界面 + 元数据

### 安装到系统目录

```bash
sudo cmake --install build
systemctl --user restart dde-shell@DDE.service
```

安装位置：

- `/usr/lib/x86_64-linux-gnu/dde-shell/org.deepin.ds.dock.lyrics.so`
- `/usr/share/dde-shell/org.deepin.ds.dock.lyrics/`（`main.qml` + `metadata.json`）

## 🔧 开发与调试

### 查看运行日志

```bash
journalctl --user -u dde-shell@DDE.service -f | grep dock-lyrics
```

关键日志含义：

```
addPlayer  …                         发现新 MPRIS 播放器
readPlayerState OK … title= 晴天      读取到播放器元数据
song changed -> fetch lyrics for …   切歌，开始取歌词
netease: no confident match…lrclib   网易云无匹配，降级到 LRCLIB
lyricsReady src= "lrclib" lines= 52  拿到歌词（local/netease/lrclib 三种来源）
```

> 🐞 **发现播放器不兼容？** 完整的排查流程、命令与症状对照表见
> [`docs/debugging-guide.md`](docs/debugging-guide.md)。

### 交互协议

- 通过 D-Bus 监听 `NameOwnerChanged`（播放器注册/退出）与 `PropertiesChanged`（`PlaybackStatus`、`Metadata`、`Position`）。
- 对**活动播放器**主动 `GetAll` 拉取一次完整元数据；Chromium 内核播放器上报的页面地址（如 `index.html#/like`）会被识别为垃圾标题并过滤，避免误取歌词。
- 播放进度优先用播放器上报的 `Position`；播放器不上报位置（如 QQ 音乐网页版 `CanSeek=false`）时，用内部时钟自 0 累计同步。
- **拖动进度条（seek）**：同时处理两种 seek 通告——规范播放器发 `Seeked` 信号，部分播放器只通过 `PropertiesChanged` 更新 `Position`；两者都会立即重置内部时钟并刷新当前歌词行，播放中、暂停时都即时同步。
  ⚠️ **Chromium 内核播放器（如 QQ 音乐桌面版）不属此类**：其 MPRIS 恒报 `CanSeek=false`、`Position=0`，既不发 `Seeked` 也不更新 `Position`，
  插件无法感知其**应用内**拖动进度条（上游 MPRIS 能力限制，见下方 FAQ）。

### 架构简图

```
┌──────────────────────────────  dde-shell（DDE）─────────────────────────────┐
│  Dock (org.deepin.ds.dock)                                                   │
│   └─ AppletItem 歌词小舟 (org.deepin.ds.dock.lyrics, dockOrder=21)           │
│        package/main.qml   跑马灯 UI · 均衡器 · 点击/悬停交互                   │
│              ▲ 播放/歌词/行/状态 (Applet.* 属性)                                │
│        src/lyricsapplet.*  DApplet 后端                                       │
│              ├─ playerprobe.*   每个播放器的 PropertiesChanged 探测            │
│              ├─ lyricsfetcher.* 本地查找 → 网易云 → LRCLIB                     │
│              └─ lrcparser.*     LRC 时间轴解析/元数据行过滤                     │
└──────────────────────────────────────────────────────────────────────────────┘
               ▲ 用户总线 (session bus) D-Bus · MPRIS
        QQ音乐 / Deepin音乐 / VLC / mock 播放器
```

## 📁 工程结构

```
CMakeLists.txt           构建脚本（ds_install_package 打包插件）
package/
  metadata.json          插件元数据（Id / Parent: org.deepin.ds.dock）
  main.qml               任务栏组件 UI（跑马灯 + 均衡器 + 交互）
src/
  lyricsapplet.*         DApplet 后端：MPRIS 监听、活动播放器决策、歌词状态机
  playerprobe.*          逐播放器 PropertiesChanged 探测（含播放器重启重连）
  lyricsfetcher.*        本地 lrc 查找 + 网易云 + LRCLIB 在线获取
  lrcparser.*            LRC 时间轴解析 / 元数据行过滤
  qdbusutil.h            QDBusArgument(a{sv}) → QVariantMap 解码工具
deb/
  build-deb.sh           一键打包 .deb
  dock-lyrics/           deb 包骨架（DEBIAN/control、postinst）
tools/
  mpris_mock.py          MPRIS 模拟播放器（无播放器时用于体验/开发）
docs/                    截图与文档
```

## ❓ 常见问题

**任务栏没有出现歌词条？**
确认插件已安装并重启 dde-shell：`systemctl --user restart dde-shell@DDE.service`；确认播放器正在“播放”（暂停时歌词条会收起，这是设计行为）；确认任务栏还有空间容纳该部件。

**播放 QQ 音乐提示“找不到歌词”？**
这通常是 QQ 音乐**自身**的歌词面板提示。对本插件而言，若 QQ 音乐某首歌本地无 `.lrc`、网易云又无版权，会经 LRCLIB 兜底取词（周杰伦等曲库基本可命中）。若仍失败，可把歌词存成 `~/Music/歌名.lrc`；或暂停再播放触发自动重试。

**歌词与歌对不上 / 显示的是页面地址？**
Chromium 内核播放器偶尔会把页面标题当媒体标题上报，插件已内置垃圾标题过滤。可尝试在播放器内重新播放一次该曲目。

**进度一直从 0 开始 / 拖 QQ 音乐进度条歌词不同步？**
部分播放器（如 Chromium 内核的 QQ 音乐桌面版）不上报 `Position`（恒为 0）、`CanSeek=false`，也不发 `Seeked`，插件只能按内部时钟从 0 累计，
无法跟随其在**应用内**拖动进度条——这是播放器侧 MPRIS 能力限制，任何 MPRIS 客户端都拿不到它的真实进度。
规范 MPRIS 播放器（Deepin 音乐、VLC、foobar2000、mpris 桥等）发 `Seeked` 或更新 `Position`，均可实时同步拖动。

**在线歌词需要联网吗？**
仅“在线歌词”需要：请保证可访问 `music.163.com` 与 `lrclib.net`。本地 `.lrc` 完全离线可用。

**想自定义位置？**
在 `package/main.qml` 修改 `dockOrder`（当前 21，落在任务栏右侧托盘区）后重新构建安装。

**发现新播放器不兼容（不显示/不同步/找不到歌词）？**
按 [`docs/debugging-guide.md`](docs/debugging-guide.md) 的“四层定位法”排查：先看日志走到哪一层，再对症状表处理。

## 📄 许可与致谢

- 代码：**GPL-3.0**（见 [LICENSE](LICENSE)）
- 在线歌词来源：网易云音乐开放搜索接口（仅个人学习测试）、**LRCLIB**（[lrclib.net](https://lrclib.net)，开放公共歌词库）
- 图标与 UI 基于 DTK / deepin 控件库

> 本项目为 Deepin 开发者（插件）竞赛作品：**歌词小舟** —— 让歌词安静地泊在任务栏上。
