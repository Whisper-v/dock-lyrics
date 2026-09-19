# 【deepin插件开发活动】🎵 歌词小舟 dock-lyrics —— 让歌词安静地泊在任务栏上

**参赛方向**：方向四 · 扩展 DDE Shell 的桌面能力
**适用系统**：deepin 25 / UOS V25（amd64）
**技术栈**：C++17 / Qt6 / dde-shell DApplet 插件框架 / QML
**开源**：GPL-3.0 ｜ GitHub：https://github.com/Whisper-v/dock-lyrics

---

## 🎯 一句话简介

一个运行在 deepin 25 任务栏上的**同步歌词插件**：任意 MPRIS 播放器一开始播放，任务栏右侧就自动浮现当前歌词并随进度滚动；暂停即安静收起。不用切窗口、不用开悬浮窗，余光一瞥就能跟着唱。

## 🖼️ 效果预览

*（发布时请上传本地图片，替换下面三行）*
![整条任务栏效果：docs/screenshot-main.png](https://raw.githubusercontent.com/Whisper-v/dock-lyrics/v1.0.0/docs/screenshot-main.png)
![任务栏歌词特写：docs/screenshot-closeup.png](https://raw.githubusercontent.com/Whisper-v/dock-lyrics/v1.0.0/docs/screenshot-closeup.png)
![运行效果：docs/dock-lyrics-preview.png](https://raw.githubusercontent.com/Whisper-v/dock-lyrics/v1.0.0/docs/dock-lyrics-preview.png)

## ✨ 核心亮点

- **播放唤醒，暂停隐身**：开始播放自动出现歌词条，暂停/停止自动收起，绝不占任务栏；
- **跑马灯实时同步**：按歌曲进度逐行切换、超长行平滑滚动；任务栏在左右（竖排）时自动收成均衡器图标；
- **三级歌词来源**：本地 `.lrc` → 网易云（精确匹配防串词）→ LRCLIB 兜底（周杰伦等网易下架曲也能命中），层层降级；
- **12 种配色 + 自定义**：跟随系统 / 6 套高亮彩色 / 4 套深色主题（自带半透明胶囊底）/ 自定义字体·背景颜色，右键即换、即时生效并记忆；
- **完整交互**：单击播放⇄暂停；悬停显示歌名/歌手/来源；右键菜单含 播放/暂停、上下曲、换色、刷新歌词；
- **播放器兼容加固**：垃圾标题过滤、切歌/重启都能识别、拖进度条实时跳歌词行（Seeked 与 Position 双通道）；
- **零守护进程、零 HTTP 服务**，纯 D-Bus 监听，装完即用。

## 📦 安装（一条命令）

```bash
# 下载本贴附件 com.github.dock-lyrics_1.0.0_amd64.zip，解压后：
sudo dpkg -i com.github.dock-lyrics_1.0.0_amd64.deb
# 如提示依赖缺失
sudo apt -f install
```

> 安装脚本会自动重启任务栏（dde-shell），**装完即用**；卸载：`sudo dpkg -r com.github.dock-lyrics`

想先体验但没播放器？仓库自带模拟器 `python3 tools/mpris_mock.py`，注册 `org.mpris.MediaPlayer2.docklyricsmock` 自动"播放"《晴天》。

## 🎮 快速上手

| 操作 | 效果 |
| --- | --- |
| 单击歌词条 | 播放 ⇄ 暂停 |
| 悬停 | 歌名 / 歌手 / 当前句 / 歌词来源 |
| 右键 | 播放控制、上一曲/下一曲、**颜色主题**、刷新歌词 |
| 无播放器播放 | 歌词条自动隐藏（设计如此） |

## 🎨 配色一览

跟随系统（默认）＋ 柠檬黄 / 苹果青 / 冰川蓝 / 霓虹紫 / 樱花粉 / 暖阳橙 ＋ 曜石黑 / 深空蓝 / 暮光紫 / 墨夜绿（深色主题自带胶囊底）＋ **自定义配色**（字体色、背景色分开选，可带透明度）。

## ⚠️ 兼容性特别说明

- ✅ 实测可用：**Deepin 音乐、VLC**、本地模拟器；QQ 音乐**播放/暂停/切歌**正常；
- ⚠️ QQ 音乐桌面版（Chromium 内核）的 MPRIS 是"桩"：`CanSeek=false`、不上报进度 → **应用内拖进度条无法同步歌词**（任何 MPRIS 客户端都拿不到它的进度，属播放器侧限制，非插件缺陷）；
- ✅ 规范播放器（Deepin 音乐、VLC、foobar2000 等）：播放进度与拖动进度条**实时同步**。

## 🛠 为什么可靠（技术要点）

- MPRIS 四层数据流定位法：发现 → 状态 → 取词 → 显示，日志逐层可查；
- 自研 `a{sv}` 递归解包（Qt 不会自动解嵌套 Metadata）、垃圾标题过滤、播放器重启重连；
- **双通道 seek 同步**：`Seeked` 信号与 `PropertiesChanged.Position` 两条路径都会立即重算歌词行，播放中/暂停都即时跳转；
- 完整调试命令与 Bug 报告模板见仓库 `docs/debugging-guide.md`。

## 🏗️ 开发历程（11 轮迭代）

脚手架 → 端到端 MPRIS 管线 → .deb 打包 + QQ 音乐实机适配 → seek 同步 → 切歌/重启兼容加固 → 调试指南 → 高亮主题 + 右键菜单 → 4 套深色主题 → 自定义字体/背景色 → 真实反馈"拖进度条不同步"实证修复（补 Position 通道）→ 开源 + v1.0.0 Release。

## 🔗 开源与下载

- **GitHub**：https://github.com/Whisper-v/dock-lyrics （GPL-3.0，含完整源码/README/调试指南）
- **v1.0.0 Release**：https://github.com/Whisper-v/dock-lyrics/releases/tag/v1.0.0
- 附件：`com.github.dock-lyrics_1.0.0_amd64.zip`（内含 .deb，SHA-256 `965496ed…36816f`）

## 🗓️ 未来计划

桌面歌词模式 / 双语歌词·逐字卡拉OK / 渐变描边与更多色板 / 规范播放器的迷你可拖进度条 / arm64。

---

喜欢的话欢迎 **⭐ Star / Issue / PR**，也欢迎在评论区报上你常用的播放器，一起把兼容矩阵撑大 🚢
