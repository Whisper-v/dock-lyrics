# 🎵 dock-lyrics（歌词小舟）开发过程说明

> 项目：deepin 25 dde-shell 任务栏同步歌词插件（DDE 插件竞赛作品）
> 开源仓库：https://github.com/Whisper-v/dock-lyrics ｜ GPL-3.0
> 开发周期：2026-09-08 集中开发（约半天完成 11 轮迭代）→ 09-09 完成缺陷修复与开源发布

---

## 1. 项目背景与动机

Deepin 25 采用全新的 dde-shell 任务栏与插件（Applet）体系，第三方可以像搭积木一样扩展任务栏能力。听歌是桌面高频场景，而"看歌词"却一直很割裂：

- 播放器自带歌词面板要么独占窗口、要么是悬浮窗，都**遮挡内容**；
- 切到别的窗口就看不到当前歌词，跟唱只能凭记忆；
- 主流方案（桌面歌词）默认置顶，**打扰**看视频/写代码。

我们想做一件事：让歌词**泊在任务栏里**——不新建窗口、不抢焦点、不打扰，播放时悄悄出现，暂停时悄悄消失，让"正在唱的那一句"永远在余光可及之处。

## 2. 设计目标

1. **MPRIS 驱动**：兼容一切遵守 MPRIS2 规范的播放器，不做播放器私有协议适配；
2. **暂停即隐身**：不与任务栏其它部件抢空间，干扰趋近于零；
3. **歌词尽量可靠**：本地 `.lrc` 优先，在线（网易云/LRCLIB）兜底；
4. **任意主题可读**：深浅色任务栏、明暗系统主题都要清晰；
5. **开箱即用**：提供 .deb 一键安装，装完自动重启任务栏生效；
6. **工程可维护**：C++ 后端 + QML 前端、逐层日志、自带 mock 播放器与调试指南，方便复现和排查播放器兼容问题。

## 3. 技术选型

| 方面 | 选型 | 理由 |
| --- | --- | --- |
| 插件形态 | dde-shell `DApplet` Dock 插件（`com.github.dock-lyrics`，dockOrder=21） | 官方插件框架，能被系统"插件区域"管理 |
| 后端 | C++17 + Qt6（Core/Gui/Network/DBus）+ `ds_install_package` | dde-shell 原生、性能好、便于做 D-Bus 状态机 |
| 前端 | QML（package/main.qml） | 跑马灯/动画/菜单表达力强，与 Dock 主题天然融合 |
| 媒体协议 | MPRIS2（session bus D-Bus，`org.mpris.MediaPlayer2.*`） | 跨播放器统一标准，一次接入处处可用 |
| 歌词来源 | 本地 `.lrc` → 网易云 → LRCLIB | 离线可用 + 在线兜底 |
| 打包 | CMake + deb（DEBIAN/control + postinst） | 一键 `dpkg -i`，postinst 自动重启 dde-shell |
| 测试 | `tools/mpris_mock.py` 模拟器 + 截屏/OCR 验证 | 无播放器也能全流程复现与回归 |

## 4. 系统架构

```
┌────────────────────────────── dde-shell（DDE）─────────────────────────────┐
│  Dock (org.deepin.ds.dock)                                                   │
│   └─ AppletItem 歌词小舟 (com.github.dock-lyrics, dockOrder=21)           │
│        package/main.qml   跑马灯 UI · 均衡器 · 点击/悬停交互 · 右键菜单        │
│              ▲ 播放/歌词/行/状态 (Applet.* 属性)                                │
│        src/lyricsapplet.*  DApplet 后端：MPRIS 监听/活动播放器/歌词状态机      │
│              ├─ playerprobe.*   逐播放器 PropertiesChanged 探测（重启重连）     │
│              ├─ lyricsfetcher.* 本地 lrc 查找 → 网易云 → LRCLIB                │
│              └─ lrcparser.*     LRC 时间轴解析 / 元数据行过滤                   │
└──────────────────────────────────────────────────────────────────────────────┘
               ▲ 用户总线 (session bus) D-Bus · MPRIS
        QQ音乐 / Deepin音乐 / VLC / mock 播放器
```

数据流按四层组织，任何"播放器不兼容"问题都能在日志里定位到具体层：
**L1 发现层**（注册 D-Bus → 被发现）→ **L2 状态层**（播放/切歌/seek/元数据）→ **L3 取词层**（本地/在线匹配歌词）→ **L4 显示层**（QML 跑马灯）。

## 5. 开发历程（11 轮迭代 + 开源收尾）

| # | 时间 | 提交 | 里程碑 | 做了什么 |
| --- | --- | --- | --- | --- |
| 1 | 09-08 19:03 | `5aae188` | 脚手架 | 搭建 DApplet 插件骨架 + 跑马灯 QML，打通编译与 dde-shell 加载 |
| 2 | 09-08 20:31 | `87c48bf` | 端到端管线 | `NameOwnerChanged` 发现播放器 → 逐播放器探测 → 三级取词 → 显示，全链路跑通 |
| 3 | 09-08 21:19 | `e3befd9` | 可用化 | .deb 打包（postinst 自动重启 dde-shell）、补 README、QQ 音乐实机适配（垃圾标题、网易无版权降级） |
| 4 | 09-08 21:39 | `990fd33` | seek 同步 v1 | 订阅 `Seeked` 信号，拖动进度条歌词实时跳行 |
| 5 | 09-08 21:48 | `74a8916` | 兼容加固 | 切歌只发 invalidated 也能识别；扩大本地 .lrc 搜索目录（QQ音乐/网易云/酷狗下载目录） |
| 6 | 09-08 22:01 | `b69be43` | 调试指南 | 沉淀"四层定位法" + 命令工具箱 + 症状对照表（docs/debugging-guide.md） |
| 7 | 09-08 22:41 | `3e73df6` | 换肤 v1 | 7 套主题色 + 右键菜单（播放/上下曲/换色/刷新） |
| 8 | 09-08 23:17 | `e683b54` | 深色主题 | 新增 4 套自带半透明胶囊底的深色主题，浅色任务栏也可读 |
| 9 | 09-08 23:55 | `27f51ba` | 自定义配色 | 字体色/背景色分开自定义，十余种色板 + 透明背景 |
| 10 | 09-09 00:35 | `910c201` | seek 同步 v2 | 修复"只更新 Position 不发 Seeked"播放器的歌词卡行问题 |
| 11 | 09-09 01:43 | `f1991d3` | 开源发布 | GPL-3.0 LICENSE、README 校对、GitHub 仓库 + v1.0.0 Release（附 .deb） |
| 12 | 09-09 | `927e10c` 等 | 社区材料 | 论坛发布文（完整版 + 精简版）、本开发过程说明 |

## 6. 关键技术难点与解决过程

### 6.1 Qt 不会自动解包嵌套 `a{sv}`（Metadata）
**现象**：Chromium 内核播放器同一播放器内切歌，歌词不更新。
**根因**：`PropertiesChanged` 信号的 `Metadata` 是嵌套 `a{sv}`，Qt `QVariantMap::toMap()` 解出来是空的，切歌变化被静默忽略；且部分播放器把 `Metadata` 放进 **invalidated 数组**而非 changed。
**解决**：
- 自研 `qdbusutil.h` 的递归解码器，把 `QDBusArgument` 里的嵌套 `a{sv}` 还原成真正的 `QVariantMap`；
- `playerprobe` 同时解析 changed 与 invalidated 两条路径；
- 一旦发现 Metadata 变化，立即对该播放器 `GetAll` 重新整读，拿到干净完整的元数据。

### 6.2 Chromium 内核播放器的 MPRIS 是"桩"
**现象**：QQ 音乐桌面版（Chromium/CEF）上报 `CanSeek=false`、`Position` 恒 0、`mpris:length` 缺失，不发 `Seeked` 也不更新 `Position`；有时还把页面地址（`index.html#/like`）当歌名。
**解决**：
- `isUsableSongTitle()` 过滤 URL、`#/` 路由、HTML 后缀、超长串；必要时用 `xesam:url` 的文件名兜底；
- 不上报 Position 的播放器用**内部时钟**从 0 累计推进播放进度；
- 应用内拖进度条无法感知 → 如实写进文档"已知边界"，不伪装成已支持。

### 6.3 网易云串词（匹配到翻唱/同名歌）
**现象**：网易云接口按关键词返回大量同名/翻唱，旧逻辑匹配过松，会选中错误歌曲的歌词。
**解决**：改为**严格匹配**——歌名完全相同，或"歌名包含 + 歌手完全相同"才采信；否则判定 `no confident match` 并自动降级到 **LRCLIB**（网易无版权的周杰伦等曲目也能命中）。

### 6.4 拖动进度条歌词不同步（两次迭代）
**v1（`990fd33`）**：最初只订阅了 `PropertiesChanged`，拖动进度条后行不跳。补订 `Seeked` 信号（带 owner 校验），收到后重置内部时钟并重算当前行。
**v2（`910c201`）**：真实反馈"拖动进度条依然不同步"。用 `dbus-monitor` 实证发现：并非所有播放器都发 `Seeked`，部分播放器只通过 `PropertiesChanged` 更新 `Position`。旧逻辑在 Position 变化时只重置时钟、**没有重算当前行**——暂停时无定时器，歌词会一直卡在旧句。
**修复**：`Position` 变化（按新值去重）也走 `jumpToPosition`，立即重算歌词行——播放中、暂停时都能即时跳转。
**验证**：改 mock 为"只发 Position 不发 Seeked"复现旧 Bug → 修复后 15s↔65.5s 反复拖动，OCR + 像素对比确认歌词行实时切换。

### 6.5 深浅主题下都可读
深色主题若沿用"高亮文字 + 无背景"，在浅色任务栏上会看不清。设计为**颜色 = 文字色 + 背景色**二元组：4 套深色主题自带 `#CC` 半透明胶囊底色（曜石黑/深空蓝/暮光紫/墨夜绿）；自定义配色更进一步把字体色与背景色解耦，各十余种色板任选。全部选择持久化到 `~/.config/deepin/dock-lyrics.conf`。

### 6.6 多播放器同时在线
`chooseActivePlayer()` 的策略：选**正在 Playing 且最近活跃**的播放器；当前 active 只是暂停则保持不变。避免被"后台挂着 Playing 却没声音"的播放器抢占。播放器进程退出/重启后，probe 会自动重连重新订阅。

## 7. 调试与验证方法

开发过程中逐步沉淀出一套可照抄的工具链（完整版见 `docs/debugging-guide.md`）：

1. **日志定位**：`journalctl --user -u dde-shell@DDE.service -f | grep dock-lyrics`，每个关键节点（addPlayer / readPlayerState / song changed / seeked to / lyricsReady / all sources failed）都有日志；
2. **MPRIS 体检**：`gdbus introspect` + `GetAll` 检查播放器的 `Metadata`/`Position`/`CanSeek`/`Seek`；
3. **信号观测**：`dbus-monitor` 盯着切歌/暂停/拖进度条，看播放器**实际发了什么**（这是判断"谁的问题"的最有力证据）；
4. **mock 复现**：`tools/mpris_mock.py`（《晴天》本地 LRC /《稻香》在线）可模拟"只发 Position"“Metadata 走 invalidated”等不规范播放器，离线回归；
5. **UI 冒烟**：截屏 + 像素分析 + OCR（chi_sim）核对歌词行与主题颜色渲染是否正确；
6. **回归清单**：播放/暂停收起、seek 跳行（播放中+暂停）、切歌刷新、断网降级、重新出包，每次改动跑一遍。

## 8. 工程管理

- **单仓库单日多轮小步提交**：13 个提交按里程碑组织，每步可编译、可安装、可回退；
- **文档与代码同步**：README（安装/使用/FAQ）+ debugging-guide（四层定位/症状表/Bug 报告模板）+ forum-post（社区发布）+ 本文档；
- **可复现构建**：CMake + `deb/build-deb.sh` 一键出包，.deb 与已安装产物做字节级一致性校验；
- **真实设备验证**：Deepin 25（dde-shell 2.x）实机 + QQ 音乐桌面版/Deepin 音乐/VLC/本地 mock 多播放器交叉测试。

## 9. 成果

- **功能**：MPRIS 播放唤醒/暂停隐身、三级歌词来源、12 配色 + 自定义字体背景色、右键菜单、双通道 seek 同步、均衡器动画；
- **兼容矩阵**：Deepin 音乐、VLC、规范 MPRIS 播放器全程实时同步；QQ 音乐（Chromium 桩）播放/切歌可用、应用内拖进度条受上游限制；
- **开源**：GPL-3.0，GitHub 公开仓库 + v1.0.0 Release（.deb，SHA-256 校验），社区可下载安装、可提 Issue/PR。

## 10. 经验与反思

1. **先体检协议，再谈兼容**：遇到"某某播放器不行"，第一件事是 `dbus-monitor` 看它到底发不发信号——很多"插件 Bug"其实是播放器 MPRIS 实现缺失，文档里把这类边界如实写清楚，比强行 workaround 更可靠；
2. **两套时钟思维**：MPRIS 里 `Seeked`（事件）与 `Position`（属性）是两条独立通道，播放器实现千差万别，客户端要"双通道都听"并在**暂停态**也要能重算状态；
3. **文档即调试**：把踩过的坑沉淀成"现象→根因→修复→新播放器如何排查"，让后续扩展播放器不再重复踩坑；
4. **UI 可测化**：用 mock + OCR + 像素对比给 QML 视觉回归加了"眼睛"，改主题/换配色也能自动化验证。

## 11. 后续计划

- 桌面歌词（可拖拽、解锚）形态；
- 双语歌词 / 逐字卡拉OK高亮；
- 渐变、描边等更多视觉方案与专辑封面色跟随；
- 规范播放器的迷你可拖进度条；
- arm64 支持与 deepin V23 兼容性验证。
