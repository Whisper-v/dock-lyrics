# 【deepin插件开发活动】🎵 歌词小舟 dock-lyrics —— 让歌词安静地泊在 Deepin 任务栏上

> **参赛方向**：方向四 · 扩展 DDE Shell 的桌面能力（`dde-shell-development`）
> **适用系统**：deepin 25 / UOS V25（amd64）
> **技术栈**：C++17 / Qt6（Core·Gui·Network·DBus）/ dde-shell `DApplet` 插件框架 / QML

![整条任务栏效果](https://raw.githubusercontent.com/Whisper-v/dock-lyrics/v1.0.0/docs/screenshot-main.png)

![任务栏歌词特写](https://raw.githubusercontent.com/Whisper-v/dock-lyrics/v1.0.0/docs/screenshot-closeup.png)

## 🎯 作品简介

dock-lyrics（歌词小舟）是一个运行在 **deepin 25 / dde-shell 任务栏**上的同步歌词插件：

- 🎶 任意支持 MPRIS 的播放器**开始播放** → 任务栏右侧自动浮现歌词条，随播放进度**实时跑马灯滚动**当前歌词；
- ⏸ **暂停 / 停止** → 歌词条安静收起，把任务栏空间还给桌面，绝不打扰；
- 📜 长句平滑滚动，短句静止居中，深浅色任务栏、明暗系统主题下都清晰可读；
- 🎚 歌词条自带 **3 根动画均衡器音柱**随播放律动；单击可播放⇄暂停，悬停可看歌名/歌手/当前句/歌词来源。

把"正在唱的那一句"直接泊进任务栏——不用切窗口、不用开悬浮窗，余光一瞥就能跟着唱。

![dock-lyrics 运行效果](https://raw.githubusercontent.com/Whisper-v/dock-lyrics/v1.0.0/docs/dock-lyrics-preview.png)

## ✨ 功能特性

| 能力 | 说明 |
| --- | --- |
| **播放唤醒** | 监听 MPRIS（`org.mpris.MediaPlayer2.*`），任一播放器开始播放即自动出现；暂停/停止自动收起 |
| **同步跑马灯** | 按歌曲进度逐行切换，超长行平滑滚动；**任意位置任务栏**：上下方显示完整歌词，左右竖排自动收起为均衡器图标 |
| **三级歌词来源** | 本地 `.lrc` → 网易云音乐（带匹配校验）→ LRCLIB 兜底，层层降级，详见下表 |
| **12 种配色 + 自定义** | 跟随系统 / 6 高亮彩色 / 4 深色主题 / 自定义字体·背景颜色（见配色章节） |
| **右键菜单** | 播放/暂停、上一曲、下一曲、颜色主题、刷新歌词；实时标注当前歌词来源 |
| **点击/悬停交互** | 单击播放⇄暂停；悬停气泡显示 歌名/歌手/当前句/歌词来源 |
| **均衡器动画** | 3 根音柱随播放律动，颜色随主题/自定义文字色同步换色 |
| **播放器兼容加固** | 垃圾标题过滤、播放器重启重连、切歌只发 invalidated 也能识别、双通道 seek 同步（见调试章节） |
| **持久化** | 主题/自定义颜色选择保存到 `~/.config/deepin/dock-lyrics.conf`，重启不丢 |

## 🎨 12 种配色 + 自定义配色

右键歌词条即可换肤，**即时生效并自动记忆**。其中 4 套深色主题自带**半透明胶囊底色**，浅色任务栏上也清晰可读：

| 类型 | 主题 | 文字色 | 背景色 |
| --- | --- | --- | --- |
| 默认 | 跟随系统 | 随任务栏主题自动 | 透明胶囊 |
| 高亮彩色 ×6 | 柠檬黄 / 苹果青 / 冰川蓝 / 霓虹紫 / 樱花粉 / 暖阳橙 | `#FFD75E` `#7EE0A3` `#8FD3FF` `#C792EA` `#FF9EC7` `#FFB877` | 默认悬停胶囊 |
| 深色主题 ×4 | 曜石黑 / 深空蓝 / 暮光紫 / 墨夜绿 | `#E6EBF7` `#9FC3FF` `#D9C2FF` `#9FE8C0` | `#CC171B22` `#CC142340` `#CC251C3E` `#CC0F2A1D` |
| 自定义 | **自定义配色** | 十余种色板任选 | 十余种色板任选（含无背景/半透明/纯黑/纯白…） |

> 自定义配色时文字与均衡器音柱同步换色，选择持久化，重启/注销后保留。

## 🎼 歌词从哪来？三级来源层层兜底

| 优先级 | 来源 | 说明 |
| --- | --- | --- |
| 1 | **本地 `.lrc`** | 正在播放的本地文件旁同名 `.lrc`；或 `~/Music`、`~/音乐`、`~/Music/Lyrics`、`~/Music/QQMusic` 等目录下 `歌名.lrc` / `歌手 - 歌名.lrc` |
| 2 | **网易云音乐** | 搜索命中并做"标题精确 + 歌手匹配"**高置信度校验**，避免匹配到翻唱/同名歌 |
| 3 | **LRCLIB** | 网易云无版权/无匹配时自动降级（如周杰伦等已下架曲目也能命中）；优先带时间轴的 `syncedLyrics` |

三个来源都失败时，歌词条退化为「歌名 - 歌手」提示"没有找到歌词"；**此时暂停再播放即可自动重试**。
本地 `.lrc` 完全离线可用，放一个同名文件即可 100% 命中。

## 🚀 安装方法

### 环境要求

| 项 | 要求 |
| --- | --- |
| 系统 | deepin 25 / UOS V25（amd64，X11） |
| 任务栏 | dde-shell ≥ 2.0（DDE 25 默认） |
| 运行依赖 | Qt6 Core/Gui/Network/DBus（系统自带） |

### 方式一：安装 .deb（推荐）

**v1.0.0 下载：** `dock-lyrics_1.0.0_amd64.deb`（本贴附件 / GitHub Release）

```bash
wget https://github.com/Whisper-v/dock-lyrics/releases/download/v1.0.0/dock-lyrics_1.0.0_amd64.deb
sudo dpkg -i dock-lyrics_1.0.0_amd64.deb
# 如提示依赖缺失
sudo apt -f install
```

> `postinst` 会自动重启当前用户的 `dde-shell@DDE.service`，**装完即用**，无需手动操作。

### 方式二：源码安装

```bash
sudo apt install cmake g++ qt6-base-dev libqt6network6 libqt6dbus6 \
     libdde-shell-dev libdde-shell-dock-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
sudo cmake --install build
systemctl --user restart dde-shell@DDE.service
```

### 卸载

```bash
sudo dpkg -r dock-lyrics            # deb 方式
systemctl --user restart dde-shell@DDE.service
```

## 📚 使用说明

| 操作 | 效果 |
| --- | --- |
| 单击歌词条 | 播放 ⇄ 暂停 |
| 悬停歌词条 | 气泡显示 歌名 / 歌手 / 当前句 / 歌词来源（本地·网易云·LRCLIB） |
| 右键歌词条 | 打开快捷菜单：播放/暂停、上一曲、下一曲、**颜色主题**（12 项）、**刷新歌词** |
| 任务栏在屏幕上下方 | 显示完整跑马灯歌词 |
| 任务栏在屏幕左/右侧（竖排） | 自动收起为均衡器图标，不挤压竖排布局 |

> 想先体验但没有播放器？仓库自带 MPRIS 模拟器：`python3 tools/mpris_mock.py`
> 会自动注册 `org.mpris.MediaPlayer2.docklyricsmock` 并"播放"《晴天》，配合 `~/Music/晴天.lrc` 走本地歌词，`Next` 可切到《稻香》走在线歌词。

## ⚙️ 配置说明

配置文件：`~/.config/deepin/dock-lyrics.conf`（INI）

| 键 | 说明 |
| --- | --- |
| `ui/colorTheme` | 配色索引：0 跟随系统；1–6 高亮彩色；7–10 深色主题；11 自定义配色 |
| `ui/textColor` | 自定义字体颜色（`ui/colorTheme=11` 时生效） |
| `ui/bgColor` | 自定义背景颜色（可为空 = 无背景） |

右键菜单切换后自动写回本文件，无需手改；重启/注销后保留。

## 🗂️ 工程结构（开源仓库）

```
CMakeLists.txt               构建脚本（ds_install_package 打包插件）
package/
  metadata.json              插件元数据（Id / Parent: org.deepin.ds.dock）
  main.qml                   任务栏组件 UI（跑马灯 + 均衡器 + 交互 + 菜单）
src/
  lyricsapplet.*             DApplet 后端：MPRIS 监听、活动播放器决策、歌词状态机
  playerprobe.*              逐播放器 PropertiesChanged 探测（含播放器重启重连）
  lyricsfetcher.*            本地 lrc 查找 + 网易云 + LRCLIB 在线获取
  lrcparser.*                LRC 时间轴解析 / 元数据行过滤
  qdbusutil.h                QDBusArgument(a{sv}) → QVariantMap 解码工具
deb/
  build-deb.sh               一键打包 .deb（含 postinst 自动重启 dde-shell）
tools/
  mpris_mock.py              MPRIS 模拟播放器（无播放器时用于体验/开发）
docs/                        截图与调试指南
```

安装到系统后的关键文件：

- `/usr/lib/x86_64-linux-gnu/dde-shell/org.deepin.ds.dock.lyrics.so`（C++ 后端）
- `/usr/share/dde-shell/org.deepin.ds.dock.lyrics/`（`main.qml` + `metadata.json`）

## 🛠️ 技术实现与播放器接入

### 架构简图

```
┌────────────────────────────── dde-shell（DDE）─────────────────────────────┐
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

### 播放器"接入"机制 —— 四层数据流

不兼容问题按数据流分四层定位，**先看日志走到哪一层，再决定改哪里**：

| 层 | 数据流 | 典型症状 |
| --- | --- | --- |
| L1 发现层 | 播放器注册 D-Bus → `NameOwnerChanged` → `addPlayer` | 日志没有该播放器的 `addPlayer` |
| L2 状态层 | `PlaybackStatus` / `Metadata` / `Position` / `Seeked` | 标题为空、切歌不更新、拖进度条不同步 |
| L3 取词层 | 歌名 → 本地/网易云/LRCLIB | 一直"没有找到歌词"、歌词张冠李戴 |
| L4 显示层 | QML 跑马灯 | 一般与播放器无关，多为布局/主题问题 |

关键日志速查：

| 日志 | 含义 |
| --- | --- |
| `addPlayer <bus>` | 发现新 MPRIS 播放器 |
| `active xxx status Playing title=…` | 播放/暂停切换 |
| `song changed -> fetch lyrics for …` | 判定切歌，开始取词 |
| `seeked to N ms` | 收到 seek 通告并跳转歌词行 |
| `netease: no confident match … lrclib` | 网易云无匹配，降级 LRCLIB |
| `lyricsReady src= local/netease/lrclib lines= N` | 取到歌词 |
| `all lyric sources failed for key …` | 三源全失败，UI 显示"没有找到歌词" |

### 关键设计与"踩过的坑"

1. **嵌套 `a{sv}` 解包**：Qt 流式接收 `PropertiesChanged` 时不会自动解包嵌套的 `Metadata`，直接 `toMap()` 会得到空值 → 自研 `qdbusutil.h` 递归解码；
2. **切歌不更新**（历史 Bug）：部分播放器把 `Metadata` 放进 invalidated 数组而非 changed → probe 同时解析 invalidated，一旦发现变化就对该播放器 `GetAll` 重新整读；
3. **拖进度条不同步**（历史 Bug，双通道修复）：规范播放器发 `Seeked` 信号；部分播放器只更新 `PropertiesChanged.Position`。两条路径现在都会**立即重置内部时钟并重算当前歌词行**——播放中和暂停时都即时跳转；
4. **垃圾标题过滤**：Chromium 内核网页播放器把页面地址（`index.html#/like`）当歌名上报 → `isUsableSongTitle()` 过滤 URL/路由/HTML 后缀/超长串，必要时退回用 `xesam:url` 文件名；
5. **多播放器决策**：`chooseActivePlayer()` 选"正在 Playing 且最近活跃"者，当前 active 仅暂停则保持，避免被后台假播放抢占；
6. **不上报 Position 的播放器**：用内部时钟从 0 累计推进；播放暂停后恢复时对之前取词失败的歌曲自动重试。

### 调试工具箱（可直接照抄）

```bash
# 1. 实时看插件日志
journalctl --user -u dde-shell@DDE.service -f | grep dock-lyrics

# 2. 列出当前会话所有 MPRIS 播放器
dbus-send --session --print-reply --dest=org.freedesktop.DBus \
  /org/freedesktop/DBus org.freedesktop.DBus.ListNames 2>/dev/null \
  | grep -o 'org\.mpris\.MediaPlayer2[^"]*'

# 3. 体检某个播放器的 MPRIS 实现（<BUS> 换成上面的名字）
gdbus introspect --session -d <BUS> -o /org/mpris/MediaPlayer2
gdbus call --session -d <BUS> -o /org/mpris/MediaPlayer2 \
  --method org.freedesktop.DBus.Properties.GetAll org.mpris.MediaPlayer2.Player

# 4. 观测播放器到底发了什么信号（切歌/暂停/拖进度条时看输出）
dbus-monitor "interface='org.freedesktop.DBus.Properties'" \
             "interface='org.mpris.MediaPlayer2.Player',member='Seeked'"
```

### 兼容性矩阵与已知边界

| 播放器 | 播放/暂停 | 切歌 | 进度推进 | 拖动 seek 同步 |
| --- | --- | --- | --- | --- |
| Deepin 音乐 / VLC / 规范 MPRIS | ✅ | ✅ | ✅ | ✅（`Seeked` 或 `Position`） |
| QQ 音乐桌面版（Chromium MPRIS） | ✅ | ✅ | ⚠️ 内部时钟估算 | ❌（见下） |
| 本地 mock 模拟器 | ✅ | ✅ | ✅ | ✅ |

> ⚠️ **Chromium 内核播放器（如 QQ 音乐桌面版）的 MPRIS 是一个"桩"**：恒报 `CanSeek=false`、`Position=0`、`mpris:length` 缺失，既不发 `Seeked` 也不更新 `Position`。因此播放/暂停/切歌可用，但**应用内拖动进度条无法被任何 MPRIS 客户端感知**——这是播放器侧能力限制，并非插件缺陷。规范播放器（Deepin 音乐、VLC、foobar2000 等）可全程实时同步。

其他已知边界（非 Bug，勿误报）：多实例 Chromium 各占一个总线名、插件只认"Playing 且最近活跃"；纯音乐/播客无词属正常；在线歌词依赖网络（`music.163.com` 与 `lrclib.net`）。

### 用 mock 复现 / 验证

```bash
python3 tools/mpris_mock.py > tools/mpris_mock.log 2>&1 &
# 把 <BUS> 换成 org.mpris.MediaPlayer2.docklyricsmock 即可像真实播放器一样操作：
# dbus-send … PlayPause / Next / Seek int64:22000000（+22 秒）
```

想复现"不规范播放器"？把 `mpris_mock.py` 的 `Seek` 里 `self.Seeked(...)` 注释掉即可复现"只发 Position 不发 Seeked"；或把切歌的 `Metadata` 改成 invalidated 数组复现旧 Bug。**完整回归清单与 Bug 报告模板见仓库 `docs/debugging-guide.md`**（发现不兼容播放器时按模板贴数据即可）。

## 🏗️ 开发过程

全程 11 轮迭代、从零到开源 Release：

1. **脚手架**：C++ MPRIS dock 插件框架 + 跑马灯 QML，打通 dde-shell 插件加载；
2. **端到端管线**：`NameOwnerChanged` 发现播放器 → 逐播放器探测 → 本地/在线取词 → 显示，整链路跑通；
3. **可用化**：打包 .deb（postinst 自动重启 dde-shell）、补 README、修 QQ 音乐实机问题（Chromium 垃圾标题、网易云无版权降级 LRCLIB）；
4. **seek 同步**：订阅 `Seeked` 信号，拖动进度条歌词实时跳转；
5. **兼容加固**：播放器切歌只发 invalidated 也能识别；扩大本地 `.lrc` 搜索目录（QQ音乐/网易云/酷狗等下载目录全覆盖）；
6. **调试指南**：沉淀"四层定位法" + 命令工具箱 + 症状对照表（即上文"技术实现与播放器接入"章节）；
7. **换肤**：7 套高亮色主题 + 右键菜单；
8. **深色主题**：新增 4 套自带胶囊底的深色主题（曜石黑/深空蓝/暮光紫/墨夜绿），浅色任务栏也可读；
9. **自定义配色**：字体颜色、背景颜色分开自定义，十余种色板 + 无/半透明/纯色背景；
10. **真实缺陷修复**：接到反馈"拖进度条仍不同步"→ 实证定位到"只更新 Position 不发 Seeked"的播放器通道缺口，补上第二条路径，播放中/暂停时都即时重算当前行；
11. **开源发布**：GPL-3.0 LICENSE、整理 README、上传 GitHub、发布 v1.0.0 Release（含 .deb）。

## 🔗 源码与开源

**GitHub：https://github.com/Whisper-v/dock-lyrics**

- 开源协议：**GPL-3.0**（OSI 批准，LICENSE 全文在仓库）
- 独立仓库，可单独构建安装；仓库自带 mock 播放器与完整调试指南
- 📦 v1.0.0 Release（含 .deb）：https://github.com/Whisper-v/dock-lyrics/releases/tag/v1.0.0
- SHA-256：`a4bc6e4c19c4a321bc2d7a8b11964db1f852a7a63ff29328b6e6b95082b23b1b`

## 🗓️ 未来计划

- **桌面歌词模式**：从任务栏"解锚"成可拖动桌面歌词（多一个 `DApplet` 形态）；
- **双语歌词 / 逐字卡拉OK**：联网取翻译，按字高亮跟唱；
- **更多视觉**：渐变/描边文字、更多自定义色板、跟随专辑封面色；
- **规范播放器增强**：在 dock 歌词条上提供可拖动的迷你进度条（主动 `Seek` 播放器，规范 MPRIS 播放器可用）；
- 支持 arm64 与 deepin V23 兼容性验证。

> 喜欢的话欢迎 ⭐ Star、提 Issue 或 PR！也欢迎在评论区反馈你用的播放器型号，一起把兼容矩阵越撑越大 🚢
