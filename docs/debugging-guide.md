# 🐞 dock-lyrics 播放器兼容性调试指南

> 适用对象：发现某个播放器（QQ音乐、网易云、酷狗、Spotify、VLC、浏览器网页播放等）
> 与歌词小舟不兼容时，按本文定位问题并给出修复。
> 仓库：`dock-lyrics`（dde-shell 插件）｜日志前缀：`[dock-lyrics]`

---

## 1. 先判断问题在哪一层

不兼容问题按数据流分四层，**每一层都有典型的症状**。绝大多数 Bug 先看日志就能
把范围缩到某一层。

| 层 | 对应代码 | 数据流 | 典型症状 |
|---|---|---|---|
| L1 发现层 | `lyricsapplet.cpp` 的 `NameOwnerChanged` / `scanPlayers`，`playerprobe.cpp` | 播放器注册到 D-Bus 被插件看到 | 日志里完全没有该播放器的 `addPlayer`；或重启播放器后歌词条无反应 |
| L2 状态层 | `lyricsapplet.cpp` 的 `PlayerState`、`qdbusutil.h` 的解包 | 播放/暂停/切歌/seek 的元数据与位置 | 标题为空/是 URL、切歌不更新、拖进度条不同步、进度从 0 起、暂停后不恢复 |
| L3 取词层 | `lyricsfetcher.cpp`（local → 网易云 → LRCLIB） | 歌名匹配到歌词文本 | 一直“没有找到歌词”、歌词张冠李戴、匹配到翻唱/同名歌 |
| L4 显示层 | `package/main.qml` 跑马灯 | 歌词文本滚动显示 | 一般与播放器无关，多为布局/主题问题 |

**定位口诀**：先看日志有没有走到对应节点，再决定改哪一层，不要一上来就改匹配算法。

---

## 2. 调试工具箱（可直接照抄的命令）

以下命令全部在 **session bus** 上操作。

### 2.1 实时看插件日志（最常用）

```bash
# 实时跟日志
journalctl --user -u dde-shell@DDE.service -f | grep dock-lyrics

# 只看最近 10 分钟
journalctl --user -u dde-shell@DDE.service --since "10 min ago" | grep dock-lyrics

# 改完代码重装后重启插件（插件随 dde-shell 一起加载）
systemctl --user restart dde-shell@DDE.service
```

### 2.2 找出当前会话里所有 MPRIS 播放器

```bash
dbus-send --session --print-reply \
  --dest=org.freedesktop.DBus /org/freedesktop/DBus \
  org.freedesktop.DBus.ListNames 2>/dev/null | grep -o 'org\.mpris\.MediaPlayer2[^"]*'
```

典型输出：

```
org.mpris.MediaPlayer2.chromium.instance13858   # QQ音乐桌面版（Chromium/CEF）
org.mpris.MediaPlayer2.docklyricsmock           # 自带的 mock 测试播放器
```

### 2.3 体检一个播放器的 MPRIS 实现

把 `<BUS>` 换成上面查到的名字：

```bash
gdbus introspect --session -d <BUS> -o /org/mpris/MediaPlayer2
```

重点看 **Player 接口**暴露了哪些能力，特别是：

- `Metadata`（`xesam:title` / `xesam:artist` / `xesam:url` 是否真实）
- `Position` 是否存在（不存在 = 无法上报进度）
- `CanSeek`（false = Chromium 网页播放器典型特征）
- `Seek` 方法是否存在（`org.mpris.MediaPlayer2.Player.Seek`）
- `Identity`（`org.mpris.MediaPlayer2` 接口）

一次性拉取全部属性：

```bash
gdbus call --session -d <BUS> -o /org/mpris/MediaPlayer2 \
  --method org.freedesktop.DBus.Properties.GetAll org.mpris.MediaPlayer2.Player
```

### 2.4 观测播放器发出的信号（最有力的证据）

开着这个窗口去 **切歌 / 暂停 / 拖进度条**，看它到底发了什么：

```bash
dbus-monitor "interface='org.freedesktop.DBus.Properties'" \
             "interface='org.mpris.MediaPlayer2.Player',member='Seeked'"
```

判定要点：

| 动作 | 规范做法 | 你该在 monitor 里看到 |
|---|---|---|
| 切歌 | `PropertiesChanged` 携带 `Metadata` 或把 `Metadata` 放入 invalidated 数组 | `Metadata` 出现，且 `a{sv}` 里能直接看到 `xesam:title` |
| 暂停/播放 | `PropertiesChanged` 携带 `PlaybackStatus` | `PlaybackStatus` = `Paused` / `Playing` |
| 拖进度条 | 发 `Seeked`（新绝对位置） | `Seeked int64 …` |
| 退出 | 名字从总线上消失 | （配合 2.2 复查） |

> ⚠️ Qt 的 `QVariantMap` 流式接收时**不会自动解包嵌套的 `a{sv}`**：若
> `Metadata` 在 monitor 里显示为 `QDBusArgument` 而不是一个个键值对，正是
> 我们修过的坑（见 §4 C）。`qdbusutil.h` 的 `qdbusVariantToMap()` 就是干这个的，
> 不要删。

### 2.5 手动操作播放器

```bash
# 播放/暂停切换
dbus-send --session --print-reply --dest=<BUS> /org/mpris/MediaPlayer2 \
  org.mpris.MediaPlayer2.Player.PlayPause

# 下一曲（部分播放器支持）
dbus-send --session --print-reply --dest=<BUS> /org/mpris/MediaPlayer2 \
  org.mpris.MediaPlayer2.Player.Next

# 模拟拖进度条（相对偏移，微秒；正数快进、负数快退）
dbus-send --session --print-reply --dest=<BUS> /org/mpris/MediaPlayer2 \
  org.mpris.MediaPlayer2.Player.Seek int64:22000000      # +22 秒
dbus-send --session --print-reply --dest=<BUS> /org/mpris/MediaPlayer2 \
  org.mpris.MediaPlayer2.Player.Seek int64:-35000000     # -35 秒
```

---

## 3. 关键日志怎么读

| 日志 | 含义 | 不正常时的提示 |
|---|---|---|
| `load()` / `init done, players: … active: …` | 插件加载、已发现的播放器 | 列表为空 → L1：没扫到播放器 |
| `addPlayer <bus>` | 新播放器注册 | 播放器开着却没这行 → L1：`NameOwnerChanged` 没触发 |
| `readPlayerState OK … title= 晴天` | `GetAll` 成功 | FAILED → L2：该播放器 GetAll 报错/超时 |
| `active xxx status Playing title=…` | 播放/暂停切换 | 状态与真实不符 → L2 |
| `song changed -> fetch lyrics for …` | 判定切歌并开始取词 | 切歌了却没这行 → L2：Metadata 变化没被识别 |
| `seeked to N ms` | 收到 `Seeked` | 拖了进度条却没这行 → L2：播放器不发 Seeked |
| `netease: no confident match / empty lyric, fallback to lrclib` | 网易云失败，转 LRCLIB | 两个都失败才真没有歌词 |
| `lyricsReady src= local/netease/lrclib lines= N` | 取到歌词 | 说明 L3 正常 |
| `all lyric sources failed for key …` | 三个来源全失败 | UI 显示“没有找到歌词” |
| `retry failed lyric lookup on resume: …` | 暂停→播放时对失败歌曲重试 | 重试仍失败才会继续“找不到” |

---

## 4. 症状对照表（含历史 Bug 的教训）

### A. 插件完全没反应 / 看不到该播放器 —— L1

排查步骤：

1. `§2.2` 确认播放器确实注册了 MPRIS 名字。
2. 看日志有没有该名字的 `addPlayer`。
3. 没有 → 确认它注册在 session bus 而非 system bus；部分沙箱应用（flatpak/snap）总线隔离，属播放器侧限制。
4. 播放器在**播放中**才可能成为 active（`chooseActivePlayer` 只认 `Playing` 且最近活跃者）。暂停时歌词条收起是设计行为，别误判。

### B. 一直“没有找到歌词” —— L3

先按顺序排除：

1. **本地 `.lrc`**：检查歌词是否在 `~/Music`、`~/音乐`、`~/Music/QQMusic`、`~/Music/网易云音乐`、`~/Music/CloudMusic` 等目录（源码 `findLocalLrc()` 里有一份完整目录清单），命名用 `歌名.lrc` 或 `歌手 - 歌名.lrc`。
2. **网易云**：日志出现 `netease: no confident match` 说明该曲网易云**无版权或无匹配**（如周杰伦，网易已下架）。这是预期行为，不是 Bug。
3. **LRCLIB**：日志出现 `lrclib: no match` 说明开放曲库也没有，可能曲子太冷门。
4. 三个来源都失败（`all lyric sources failed`）且网络正常 → 报给开发者并附 §6 的探针数据。
5. 若是**之前失败、后来网络恢复**：暂停再播放即可触发自动重试（`m_lyricFailed` 按歌曲 key 缓存，`resume` 时清除并重试）。

### C. 同一播放器内切歌，歌词不更新 —— L2（历史 Bug）

- 现象：QQ音乐桌面版（Chromium）**不换播放器只换歌**，歌词一直停在上一首。
- 根因：`PropertiesChanged` 里嵌套 `a{sv}` 的 `Metadata` 用 `toMap()` 解出来是空的，
  而 Qt 不会自动解包，导致切歌变化被忽略。
- 现修复：`playerprobe` 同时解析 invalidated 数组；`lyricsapplet` 一旦发现 Metadata
  变化就对该播放器 `GetAll` 重新整读（`readPlayerState`），得到干净完整的元数据。
- 排查新播放器时：若日志里**切歌没有** `song changed -> fetch lyrics`，用 §2.4 看它
  `Metadata` 到底怎么发的（走 changed 还是 invalidated），据此扩展解析。

### D. 拖进度条歌词不同步 —— L2（历史 Bug）

- 现象：歌词行不跟随进度条跳动。
- 根因：只监听了 `PropertiesChanged`，没监听 `Seeked` 信号。
- 现修复：`playerprobe` 订阅 `Seeked`（带 owner 校验），`lyricsapplet` 收到后
  `jumpToPosition` 重置内部时钟并立刻重算当前行，暂停中拖动也生效。
- 排查新播放器时：`dbus-monitor` 里拖进度条看是否发 `Seeked`。**规范播放器必发**；
  若某播放器不发 `Seeked` 也不发 `Position`，则只能靠内部时钟估算（精度有限，见 §7 已知边界）。

### E. 显示的是页面地址 / `index.html#/xxx` —— L2（历史 Bug）

- 根因：Chromium 内核网页播放器把页面标题当媒体标题上报。
- 现修复：`isUsableSongTitle()` 过滤 URL、`#/` 路由、`index.` 前缀、HTML 后缀、超长串；
  再退回用 `xesam:url` 的文件名做标题。
- 排查新播放器：日志里 `title=` 出现垃圾串时，把它贴进 `isUsableSongTitle()` 的判定
  条件里扩展。

### F. 多个播放器同时开，歌词是错的那个 —— L2

- 策略：`chooseActivePlayer()` 选择**正在 Playing 且最近活跃**的那个；若当前 active
  只是暂停则保持它。
- 排查：看 `init done … active: …` 与实际哪个在响；若播放器 A 假播放（后台挂着
  Playing 但没声音），会导致它抢占。可考虑在 `chooseActivePlayer` 中加入对该类播放器
  的惩罚/黑名单。

### G. 匹配到翻唱/同名歌 —— L3（历史 Bug）

- 根因：网易云接口按关键词返回一堆同名/翻唱，旧逻辑匹配过松，选了错误封面歌
  （如“我爱你 by 周杰伦♚”这种翻唱）。
- 现修复：**严格匹配**——歌名完全相同，或“歌名包含 + 歌手完全相同”才采信；
  否则 `netease: no confident match, fallback to lrclib`。
- 排查新来源/新播放器时：若歌词不对，先看日志里 `requestLyrics key= 歌名␟歌手`
  的 key 是否真确；再确认网易与 LRCLIB 各自匹配到了哪首。

---

## 5. 用 mock 播放器复现 / 验证

自带 `tools/mpris_mock.py`，总线名 `org.mpris.MediaPlayer2.docklyricsmock`，
默认播放《晴天》（本地 `~/Music/晴天.lrc`，11 行），`Next` 在 晴天/稻香 间切换。

```bash
# 启动（终端挂着）
python3 tools/mpris_mock.py

# 或后台运行，日志写到文件
python3 tools/mpris_mock.py > tools/mpris_mock.log 2>&1 &
```

操作与真实播放器一致（§2.5），把 `<BUS>` 换成
`org.mpris.MediaPlayer2.docklyricsmock`。`Seek` 会同时发 `Seeked` 并更新
`Position`，与规范播放器行为一致。

**想要新场景？** 编辑 `tools/mpris_mock.py`：

- 换歌：改 `self.title/artist/album/length` 与 `_meta()`；或在 `Next` 里加曲目表。
- 模拟“不规范的播放器”：注释掉 `Seek` 里的 `self.Seeked(...)`，即可复现 §4 D 的旧 Bug。
- 模拟“切歌只发 invalidated”：把 `Next` 的 `_notify({"Metadata": …})` 改成
  `PropertiesChanged(iface, {}, ["Metadata"])`。

改完重启 mock，日志侧同步 `journalctl … -f | grep dock-lyrics`。

---

## 6. Bug 报告模板（反馈给开发者时贴这些）

```
播放器：QQ音乐 桌面版 5.x（Chromium MPRIS）
系统：Deepin 25 / dde-shell 2.0.52 / X11

1) 总线名：org.mpris.MediaPlayer2.chromium.instance13858
2) GetAll(Player) 摘录：
   PlaybackStatus=Playing  Position=?  CanSeek=?
   xesam:title=?  xesam:artist=?  xesam:url=?
3) 复现步骤：打开某歌 → 拖进度条到 01:30 → …
4) 期望：歌词行跳转到对应时间
   实际：卡在上一行 / 一直显示“没有找到歌词”
5) 插件日志（grep dock-lyrics 摘录）：
   …
6) dbus-monitor 在出问题时捕获的信号摘录：
   …
```

---

## 7. 已知边界（不是 Bug，别误报）

- **Chromium 网页播放器**普遍 `CanSeek=false`、不上报 `Position` → 只能内部时钟从 0
  累计，且无法响应 seek 校准。
- **多实例 Chromium**：多个页面/窗口各占一个 MPRIS 名字且都叫 `chromium.instance*`，
  插件无法判断哪个在发声，只认“Playing 且最近活跃”。
- **纯音乐/播客/无词曲**：任何来源都取不到词属正常。
- **在线歌词依赖网络**：网易云（`music.163.com`）与 LRCLIB（`lrclib.net`）不可达时
  只有本地 `.lrc` 可用。

---

## 8. 修复后回归清单

每次改完，跑一遍下面的矩阵再交付：

1. `cmake --build build -j"$(nproc)"` && `sudo cmake --install build` &&
   `systemctl --user restart dde-shell@DDE.service`
2. mock：本地词《晴天》显示且滚动 ✔
3. mock：`PlayPause` 收起/展开 ✔
4. mock：`Seek ±N` 后歌词行立即跳转（播放中 + 暂停中各一次）✔
5. mock：`Next` 切歌（稻香→晴天）歌词刷新 ✔
6. 真实播放器：播放中切歌 ✔　7. 真实播放器：拖进度条 ✔
8. 断网时在线歌词优雅降级（显示“没有找到歌词”，不崩溃）✔
9. `bash deb/build-deb.sh` 重新出包；`git add -A && git commit`
