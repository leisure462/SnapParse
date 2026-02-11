# Windows 划词监控与悬浮图标技术文档（基于 NextAI Translator）

## 1. 文档目标

这份文档用于沉淀一套可复用的“划词 -> 显示小图标 -> 点击图标打开翻译窗口”的桌面端实现方案，重点覆盖：

- 鼠标划词事件识别
- 选中文本抓取
- 悬浮图标（thumb）显示与点击命中
- 与翻译主窗口的数据联动
- Windows 原生接口在本项目中的使用点

本文基于当前仓库代码整理，可作为你后续开发类似划词功能的蓝本。

---

## 2. 代码入口与职责总览

### 2.1 后端（Rust / Tauri）

- 全局鼠标监听与划词判定：`src-tauri/src/main.rs:141`
- 划词后显示小图标（thumb）窗口：`src-tauri/src/windows.rs:216`
- 小图标窗口创建/定位：`src-tauri/src/windows.rs:221`
- 点击图标后打开翻译窗口并注入文本：`src-tauri/src/main.rs:293`
- 快捷键触发“读取选中文本并打开翻译窗”：`src-tauri/src/windows.rs:123`
- Windows 窗口抢焦点（Win32 API）：`src-tauri/src/insertion.rs:128`

### 2.2 前端（React / Tauri Webview）

- `thumb` 窗口渲染图标：`src/tauri/windows/ThumbWindow.tsx:1`
- 翻译窗口监听文本注入事件 `change-text`：`src/tauri/windows/TranslatorWindow.tsx:81`
- 快捷键绑定（触发后端命令）：`src/tauri/utils.ts:31`

---

## 3. 现有实现的核心流程（鼠标划词 -> 小图标 -> 翻译）

## 3.1 初始化：绑定全局鼠标 Hook

启动后在 `Ready` 事件里调用 `bind_mouse_hook()`：

- 入口：`src-tauri/src/main.rs:519`
- 绑定函数：`src-tauri/src/main.rs:141`
- 使用库：`mouce`（跨平台封装的鼠标事件监听）

监听的事件是：

- 左键按下：`MouseEvent::Press(Left)`
- 左键释放：`MouseEvent::Release(Left)`

## 3.2 按下阶段：记录时间戳

在 `Press(Left)` 中记录本次按下时间：`PREVIOUS_PRESS_TIME`。

- 代码：`src-tauri/src/main.rs:157`
- 全局状态：`src-tauri/src/main.rs:62`

若配置 `always_show_icons` 为 false，直接不走划词图标逻辑。

- 配置读取：`src-tauri/src/main.rs:158`

## 3.3 释放阶段：判定是否属于“划词事件”

在 `Release(Left)` 中计算以下信号：

- 本次鼠标位置 `(x, y)` 与上次释放位置的距离（`mouse_distance`）
- 按下持续时长（`pressed_time`）
- 是否双击（短间隔 + 小位移）

判定逻辑（当前实现阈值）：

- 长按拖动：`pressed_time > 300ms && mouse_distance > 20`
- 双击选词：`release间隔 < 700ms && mouse_distance < 10`

相关代码：`src-tauri/src/main.rs:176` 到 `src-tauri/src/main.rs:206`

## 3.4 命中“小图标点击”与“普通释放”的分流

释放时会额外判断是否点击在 thumb 窗口矩形范围内（含 DPI/缩放处理）：

- 命中检测：`src-tauri/src/main.rs:207`
- 对窗口坐标与尺寸做 scale_factor 处理：`src-tauri/src/main.rs:211`

分支：

- 非划词且非点击图标：关闭 thumb（`close_thumb`）
- 划词且非点击图标：读取选中文本，若非空则显示 thumb
- 点击图标：关闭 thumb，打开翻译窗口并注入已缓存文本

对应代码：

- 关闭 thumb：`src-tauri/src/main.rs:257`
- 读取文本并显示 thumb：`src-tauri/src/main.rs:271`
- 点击 thumb 打开翻译：`src-tauri/src/main.rs:293`

## 3.5 选中文本抓取

当前鼠标划词路径下，使用 `get-selected-text` crate：

- 调用点：`src-tauri/src/main.rs:282`
- 另一个调用点（快捷键路径）：`src-tauri/src/windows.rs:131`

如果抓到非空文本：

- 写入全局缓存 `SELECTED_TEXT`：`src-tauri/src/main.rs:285`
- 显示小图标窗口：`src-tauri/src/main.rs:287`

## 3.6 小图标窗口（thumb）如何显示

`show_thumb(x, y)` 负责显示；`get_thumb_window(x, y)` 负责创建/复用：

- 显示入口：`src-tauri/src/windows.rs:216`
- 创建入口：`src-tauri/src/windows.rs:221`

窗口特征：

- 固定 20x20，无边框、不可调整大小、跳过任务栏
- 常驻顶层 `always_on_top`
- 位置在鼠标点基础上加偏移（`+7`）

关键配置：

- 尺寸：`src-tauri/src/windows.rs:241`
- 跳过任务栏：`src-tauri/src/windows.rs:246`
- Windows 下使用 `WS_POPUP` 样式：`src-tauri/src/windows.rs:262`

前端渲染内容就是一个图标图片：

- `src/tauri/windows/ThumbWindow.tsx:1`

## 3.7 点击图标后如何把文本送到翻译窗

当命中 thumb 点击：

1. 关闭 thumb：`src-tauri/src/main.rs:293`
2. 取缓存文本 `SELECTED_TEXT`：`src-tauri/src/main.rs:294`
3. 打开翻译窗口：`src-tauri/src/main.rs:297`
4. 发送事件 `change-text`：`src-tauri/src/main.rs:298`（内部调用 `utils::send_text`）

前端翻译窗口监听 `change-text`，并更新输入文本：

- 监听点：`src/tauri/windows/TranslatorWindow.tsx:81`

---

## 4. 快捷键路径（无图标，直接抓取选中并弹窗）

除了鼠标划词图标路径，还支持快捷键直接翻译选中文本：

1. 前端注册全局快捷键：`src/tauri/utils.ts:31`
2. 快捷键触发后端命令 `show_translator_window_with_selected_text_command`
3. 后端调用 `get_selected_text()` 读取选中内容
4. 打开翻译窗口并发送文本

后端实现入口：`src-tauri/src/windows.rs:123`

这个路径是“无 thumb 图标”快速翻译链路，适合补充鼠标方案。

---

## 5. Windows 原生接口在本项目中的使用点

## 5.1 已直接调用 Win32 API 的代码（当前仓库）

当前项目对“窗口抢焦点/前置”使用了 Win32 API，位于：

- `src-tauri/src/insertion.rs:128`

涉及 API：

- `IsIconic`
- `ShowWindow(SW_RESTORE)`
- `GetForegroundWindow`
- `GetWindowThreadProcessId`
- `AttachThreadInput`
- `BringWindowToTop`
- `SetForegroundWindow`

这部分用于把焦点切回目标窗口后插入翻译文本，不是“划词监控”本身，但与完整交互链路强相关。

## 5.2 划词监控本身的现状

当前“划词监控”并未直接写 Win32 低级鼠标钩子，而是：

- 用 `mouce` 做全局鼠标事件监听
- 用 `get-selected-text` 获取选中文本

也就是说：

- 监控层是跨平台封装
- Win32 原生 API 在此路径中不是直接调用（由库内部或其他路径处理）

---

## 6. 用于后续复用的“纯 Windows 原生”实现建议

如果你后续要做“纯 Win32 可控版本”，建议分三层：

1. **输入监控层**：`SetWindowsHookExW(WH_MOUSE_LL)` 捕获左键按下/释放
2. **文本提取层**：优先 UI Automation（TextPattern），失败回退 Ctrl+C + Clipboard
3. **UI反馈层**：显示 20x20 无边框置顶图标窗口，点击后触发业务窗口

下面给一个最小化骨架（示意，非完整生产代码）：

```rust
use windows::Win32::Foundation::{HINSTANCE, HWND, LPARAM, LRESULT, WPARAM};
use windows::Win32::UI::WindowsAndMessaging::{
    CallNextHookEx, DispatchMessageW, GetMessageW, SetWindowsHookExW, TranslateMessage,
    UnhookWindowsHookEx, HC_ACTION, HHOOK, MSG, WH_MOUSE_LL, WM_LBUTTONDOWN, WM_LBUTTONUP,
};

static mut MOUSE_HOOK: Option<HHOOK> = None;

extern "system" fn mouse_proc(code: i32, wparam: WPARAM, lparam: LPARAM) -> LRESULT {
    if code == HC_ACTION as i32 {
        match wparam.0 as u32 {
            WM_LBUTTONDOWN => {
                // 记录 press 时间和位置
            }
            WM_LBUTTONUP => {
                // 计算拖动/双击阈值 -> 判定是否疑似划词
                // 若命中，异步读取 selected text，再决定是否显示 thumb
            }
            _ => {}
        }
    }
    unsafe { CallNextHookEx(None, code, wparam, lparam) }
}

fn run_mouse_hook_loop() {
    unsafe {
        let hook = SetWindowsHookExW(WH_MOUSE_LL, Some(mouse_proc), HINSTANCE::default(), 0)
            .expect("SetWindowsHookExW failed");
        MOUSE_HOOK = Some(hook);

        let mut msg = MSG::default();
        while GetMessageW(&mut msg, HWND(0 as _), 0, 0).into() {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if let Some(h) = MOUSE_HOOK.take() {
            let _ = UnhookWindowsHookEx(h);
        }
    }
}
```

---

## 7. 关键状态与并发控制（可复用）

当前实现里这几个状态非常关键：

- `PREVIOUS_PRESS_TIME`：上次按下时间
- `PREVIOUS_RELEASE_TIME`：上次释放时间
- `PREVIOUS_RELEASE_POSITION`：上次释放坐标
- `SELECTED_TEXT`：缓存选中文本
- `RELEASE_THREAD_ID`：避免并发重复处理释放事件

定义位置：`src-tauri/src/main.rs:61`

建议沿用这种“全局状态 + mutex + 单次释放线程锁”的方式，防止：

- 快速多次释放导致重复弹窗
- 读取文本和窗口显示顺序错乱

---

## 8. 常见坑与调参建议

1. **DPI 缩放命中偏移**
   - thumb 点击命中一定要按 `scale_factor` 修正，参考 `src-tauri/src/main.rs:211`。

2. **阈值过敏或迟钝**
   - `300ms/20px/700ms` 只是经验值，建议按用户群微调。

3. **选中文本为空**
   - 某些应用不暴露标准选区；需要 UIA + Clipboard 双通道回退。

4. **窗口抢焦点失败**
   - Windows 焦点规则严格，参考 `AttachThreadInput + SetForegroundWindow` 组合。

5. **图标窗口误触**
   - 建议 thumb 显示后增加短暂 debounce，避免与释放事件冲突。

---

## 9. 一页式流程图（便于复刻）

```text
App Ready
  -> bind_mouse_hook
    -> Left Press: 记录 press_time
    -> Left Release:
         计算距离/时长/双击
         -> 非划词 & 非点thumb: close_thumb
         -> 划词候选:
              get_selected_text
              -> 空: close_thumb
              -> 非空: cache SELECTED_TEXT + show_thumb(x,y)
         -> 点中thumb:
              close_thumb
              show_translator_window
              emit(change-text, SELECTED_TEXT)
              前端 TranslatorWindow listen(change-text) 更新输入
```

---

## 10. 你后续开发时可直接复用的最小清单

- 复用本项目的 `bind_mouse_hook` 判定骨架：`src-tauri/src/main.rs:141`
- 复用 thumb 窗口参数（20x20 + 无边框 + 置顶）：`src-tauri/src/windows.rs:221`
- 复用事件总线模式（后端 emit，前端 listen）：`utils::send_text` + `TranslatorWindow`
- Windows 焦点切换复用：`src-tauri/src/insertion.rs:128`

如果你要做“全原生 Win32 版本”，建议把第 6 节的 hook 框架拉出来单独做一个 `selection_monitor.rs`，再把当前 `main.rs` 的判定和窗口联动逻辑迁移进去。
